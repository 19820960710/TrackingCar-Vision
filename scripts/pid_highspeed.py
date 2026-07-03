import re
import sys
import time
from statistics import mean

import serial

TEL_RE = re.compile(r"TEL\s+(.*)")
PAIR_RE = re.compile(r"(\w+)=(-?\d+)")


def parse_tel(line: str):
    m = TEL_RE.search(line)
    if not m:
        return None
    data = {k: int(v) for k, v in PAIR_RE.findall(m.group(1))}
    return data if data else None


def send(ser, cmd: str):
    ser.write((cmd + "\r\n").encode("ascii"))
    ser.flush()
    print(f">>> {cmd}", flush=True)


def collect(ser, duration_s: float):
    records = []
    end = time.monotonic() + duration_s
    while time.monotonic() < end:
        raw = ser.readline()
        if not raw:
            continue
        text = raw.decode("utf-8", errors="replace").strip()
        if text:
            print(text.encode("ascii", errors="replace").decode("ascii"), flush=True)
        tel = parse_tel(text)
        if tel:
            records.append(tel)
            if tel.get("estop", 0):
                print("!!! ESTOP detected", flush=True)
                send(ser, "SPD 0")
                return records, False
    return records, True


def analyze(name, records, target):
    samples = [r for r in records if r.get("tgt") == target and not r.get("estop", 0)]
    if len(samples) < 5:
        print(f"ANALYZE {name}: insufficient samples n={len(samples)}")
        return
    t0 = samples[0].get("t", 0)
    active = [r for r in samples if r.get("t", 0) - t0 >= 300] or samples
    tail = active[-min(10, len(active)):]
    print(f"ANALYZE {name} target={target} n={len(active)}")
    for side in ("l", "r"):
        vals = [r.get(side, 0) for r in active]
        tail_vals = [r.get(side, 0) for r in tail]
        final_avg = mean(tail_vals)
        if target >= 0:
            peak = max(vals)
            overshoot = peak - target
            reached = next((r.get("t", 0) - t0 for r in active if r.get(side, 0) >= 0.9 * target), None)
        else:
            peak = min(vals)
            overshoot = target - peak
            reached = next((r.get("t", 0) - t0 for r in active if r.get(side, 0) <= 0.9 * target), None)
        print(f"  {side}: final_avg={final_avg:.1f}, err={target-final_avg:.1f}, peak={peak}, overshoot={overshoot:.1f}, t90_ms={reached}")


def main():
    plan = [
        ("zero", "SPD 0", 1.0, 0),
        ("step_0_to_200", "SPD 200", 4.0, 200),
        ("step_200_to_300", "SPD 300", 4.0, 300),
        ("step_300_to_-200", "SPD -200", 5.0, -200),
        ("step_-200_to_-300", "SPD -300", 4.0, -300),
        ("stop", "SPD 0", 2.0, 0),
    ]
    segments = []
    with serial.Serial("COM19", 115200, timeout=0.2) as ser:
        print("OPEN COM19 115200")
        time.sleep(0.4)
        ser.reset_input_buffer()
        send(ser, "CLR")
        collect(ser, 0.6)
        send(ser, "PIDM 80 50 0")
        collect(ser, 0.8)
        for name, cmd, dur, target in plan:
            send(ser, cmd)
            records, ok = collect(ser, dur)
            segments.append((name, records, target))
            if not ok:
                break
        send(ser, "SPD 0")
    for seg in segments:
        analyze(*seg)
    return 0


if __name__ == "__main__":
    sys.exit(main())
