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
    data = {}
    for k, v in PAIR_RE.findall(m.group(1)):
        data[k] = int(v)
    return data if data else None


def send(ser, cmd: str):
    payload = (cmd + "\r\n").encode("ascii")
    ser.write(payload)
    ser.flush()
    print(f">>> {cmd}", flush=True)


def collect(ser, duration_s: float, records: list):
    end = time.monotonic() + duration_s
    while time.monotonic() < end:
        raw = ser.readline()
        if not raw:
            continue
        text = raw.decode("utf-8", errors="replace").strip()
        if text:
            print(text, flush=True)
        tel = parse_tel(text)
        if tel is not None:
            records.append(tel)
            if tel.get("estop", 0):
                print("!!! ESTOP detected, aborting trial", flush=True)
                return False
    return True


def analyze(records, target):
    samples = [r for r in records if r.get("tgt") == target and not r.get("estop", 0)]
    if len(samples) < 5:
        print(f"ANALYZE target={target}: not enough samples ({len(samples)})")
        return
    t0 = samples[0].get("t", 0)
    active = [r for r in samples if r.get("t", 0) - t0 >= 200]
    if not active:
        active = samples
    tail = active[-min(20, len(active)):]
    for side in ("l", "r"):
        vals = [r.get(side, 0) for r in active]
        tail_vals = [r.get(side, 0) for r in tail]
        final_avg = mean(tail_vals)
        max_val = max(vals) if target >= 0 else min(vals)
        err = target - final_avg
        overshoot = (max_val - target) if target >= 0 else (target - max_val)
        print(
            f"ANALYZE target={target} {side}: final_avg={final_avg:.2f}, "
            f"err={err:.2f}, peak={max_val}, overshoot={overshoot:.2f}, n={len(vals)}"
        )


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--port", default="COM19")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--kp", default="0.800")
    parser.add_argument("--ki", default="0.100")
    parser.add_argument("--kd", default="0")
    parser.add_argument("--target", type=int, default=10)
    args = parser.parse_args()

    records = []
    with serial.Serial(args.port, args.baud, timeout=0.2) as ser:
        print(f"OPEN {args.port} {args.baud}")
        time.sleep(0.5)
        ser.reset_input_buffer()
        send(ser, "CLR")
        collect(ser, 0.8, records)
        kp_m = int(round(float(args.kp) * 1000))
        ki_m = int(round(float(args.ki) * 1000))
        kd_m = int(round(float(args.kd) * 1000))
        send(ser, f"PIDM {kp_m} {ki_m} {kd_m}")
        collect(ser, 0.8, records)
        send(ser, "SPD 0")
        collect(ser, 1.0, records)
        send(ser, f"SPD {args.target}")
        ok = collect(ser, 6.0, records)
        send(ser, "SPD 0")
        collect(ser, 2.0, records)
    analyze(records, args.target)
    if not ok:
        return 2
    return 0


if __name__ == "__main__":
    sys.exit(main())
