import argparse
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
            safe = text.encode("ascii", errors="replace").decode("ascii")
            print(safe, flush=True)
        tel = parse_tel(text)
        if tel:
            records.append(tel)
            if tel.get("estop", 0):
                print("!!! ESTOP detected", flush=True)
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
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", default="COM19")
    ap.add_argument("--baud", type=int, default=115200)
    args = ap.parse_args()

    plan = [
        ("zero", "SPD 0", 1.0, 0),
        ("step_0_to_60", "SPD 60", 5.0, 60),
        ("step_60_to_80", "SPD 80", 5.0, 80),
        ("step_80_to_-60", "SPD -60", 6.0, -60),
        ("stop", "SPD 0", 2.0, 0),
    ]

    all_segments = []
    with serial.Serial(args.port, args.baud, timeout=0.2) as ser:
        print(f"OPEN {args.port} {args.baud}")
        time.sleep(0.4)
        ser.reset_input_buffer()
        send(ser, "CLR")
        collect(ser, 0.6)
        send(ser, "PIDM 80 50 0")
        collect(ser, 0.8)
        for name, cmd, dur, target in plan:
            send(ser, cmd)
            records, ok = collect(ser, dur)
            all_segments.append((name, records, target))
            if not ok:
                send(ser, "SPD 0")
                return 2
    for item in all_segments:
        analyze(*item)
    return 0


if __name__ == "__main__":
    sys.exit(main())
