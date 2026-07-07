#!/usr/bin/env python3
"""
速度内环预检查脚本，用于 yaw 调试前确认左右轮速度环是否明显拖慢/不一致。

典型用法：
  .venv/Scripts/python.exe scripts/yaw_speed_inner_check.py --port COM19 --quiet

固件需支持 WHEEL <left> <right> 命令；TEL 中建议包含 wl/wr/ls/rs/l/r/lp/rp。
"""

import argparse
import re
import sys
import time
from statistics import mean

try:
    import serial
except ImportError:
    print("缺少 pyserial：请执行 .venv/Scripts/python.exe -m pip install pyserial", file=sys.stderr)
    raise

TEL_RE = re.compile(r"TEL\s+(.*)")
PAIR_RE = re.compile(r"(\w+)=(-?\d+)")


def parse_tel(line: str):
    match = TEL_RE.search(line)
    if not match:
        return None
    data = {key: int(value) for key, value in PAIR_RE.findall(match.group(1))}
    required = {"seq", "t", "estop", "l", "r"}
    return data if required.issubset(data) else None


def safe_print(text: str):
    print(text.encode("ascii", errors="replace").decode("ascii"), flush=True)


def send(ser, cmd: str):
    ser.write((cmd + "\r\n").encode("ascii"))
    ser.flush()
    print(f">>> {cmd}", flush=True)


def collect(ser, duration_s: float, verbose: bool):
    records = []
    end = time.monotonic() + duration_s
    while time.monotonic() < end:
        raw = ser.readline()
        if not raw:
            continue
        text = raw.decode("utf-8", errors="replace").strip()
        if text and verbose:
            safe_print(text)
        tel = parse_tel(text)
        if tel is None:
            continue
        records.append(tel)
        if tel.get("estop", 0):
            print("!!! ESTOP detected; stopping", flush=True)
            send(ser, "STOP")
            return records, False
    return records, True


def parse_plan(text: str):
    plan = []
    for index, item in enumerate(text.split(",")):
        item = item.strip()
        if not item:
            continue
        if ":" in item:
            target_s, duration_s = item.split(":", 1)
            duration = float(duration_s)
        else:
            target_s = item
            duration = 4.0
        if "/" in target_s:
            left_s, right_s = target_s.split("/", 1)
            left = int(left_s)
            right = int(right_s)
        else:
            left = right = int(target_s)
        plan.append((f"step_{index}_{left}_{right}", left, right, duration))
    return plan


def first_reach_ms(samples, side: str, target: int):
    if not samples or target == 0:
        return None
    t0 = samples[0].get("t", 0)
    threshold = 0.9 * target
    if target > 0:
        for record in samples:
            if record.get(side, 0) >= threshold:
                return record.get("t", 0) - t0
    else:
        for record in samples:
            if record.get(side, 0) <= threshold:
                return record.get("t", 0) - t0
    return None


def analyze_step(name: str, records: list[dict], left_target: int, right_target: int):
    if len(records) < 5:
        print(f"ANALYZE {name}: insufficient samples n={len(records)}")
        return
    t0 = records[0].get("t", 0)
    active = [r for r in records if r.get("t", 0) - t0 >= 300] or records
    tail = active[-min(20, len(active)):]
    print(f"ANALYZE {name} target_l={left_target} target_r={right_target} n={len(active)}")

    for side, target, pwm_key in (("l", left_target, "lp"), ("r", right_target, "rp")):
        values = [r.get(side, 0) for r in active]
        tail_values = [r.get(side, 0) for r in tail]
        final_avg = mean(tail_values)
        if target >= 0:
            peak = max(values)
            overshoot = peak - target
        else:
            peak = min(values)
            overshoot = target - peak
        t90 = first_reach_ms(active, side, target)
        pwm_values = [abs(r.get(pwm_key, 0)) for r in active if pwm_key in r]
        max_pwm = max(pwm_values) if pwm_values else None
        sat_ratio = None
        if pwm_values:
            sat_ratio = sum(1 for value in pwm_values if value >= 76) / len(pwm_values)
        sat_text = "" if sat_ratio is None else f", max_pwm={max_pwm}, sat80={sat_ratio:.0%}"
        print(
            f"  {side}: final_avg={final_avg:.1f}, err={target - final_avg:.1f}, "
            f"peak={peak}, overshoot={overshoot:.1f}, t90_ms={t90}{sat_text}"
        )


def main():
    parser = argparse.ArgumentParser(description="Wheel speed inner-loop checker")
    parser.add_argument("--port", default="COM19")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--plan", default="0:1,60:4,80:4,-60:5,0:2")
    parser.add_argument("--quiet", action="store_true")
    args = parser.parse_args()

    verbose = not args.quiet
    plan = parse_plan(args.plan)
    segments = []

    with serial.Serial(args.port, args.baud, timeout=0.2) as ser:
        print(f"OPEN {args.port} {args.baud}")
        time.sleep(0.5)
        ser.reset_input_buffer()
        send(ser, "CLR")
        collect(ser, 0.8, verbose)

        ok = True
        for name, left, right, duration in plan:
            send(ser, f"WHEEL {left} {right}")
            records, ok = collect(ser, duration, verbose)
            segments.append((name, records, left, right))
            if not ok:
                break

        send(ser, "STOP")
        collect(ser, 1.0, verbose)

    for segment in segments:
        analyze_step(*segment)
    return 0 if ok else 2


if __name__ == "__main__":
    raise SystemExit(main())
