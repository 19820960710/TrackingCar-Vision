#!/usr/bin/env python3
"""
Yaw PID 串口自动调试脚本。

典型用法：
  .venv/Scripts/python.exe scripts/yaw_pid_tune.py --port COM19 --quiet
  .venv/Scripts/python.exe scripts/yaw_pid_tune.py --port COM19 --targets 0 --hold 60 --quiet

固件需支持命令：CLR/PIDY/OUTY/MINY/DBY/IZONEY/ILIMY/BASE/START/YAW10/STOP
遥测格式：
  TEL seq=... t=... estop=0 en=1 att=1 base=0 tgt=900 yaw=870 err=30 turn=12 ...
"""

import argparse
import math
import re
import sys
import time
from statistics import mean, pstdev

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
    required = {"seq", "t", "estop", "tgt", "yaw", "err", "turn"}
    return data if required.issubset(data) else None


def safe_print(text: str):
    print(text.encode("ascii", errors="replace").decode("ascii"), flush=True)


def send(ser, cmd: str, verbose: bool = True):
    ser.write((cmd + "\r\n").encode("ascii"))
    ser.flush()
    if verbose:
        print(f">>> {cmd}", flush=True)


def collect(ser, duration_s: float, verbose: bool = True):
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
            print("!!! ESTOP detected; sending STOP", flush=True)
            send(ser, "STOP", verbose=True)
            return records, False
    return records, True


def wait_attitude_ready(ser, timeout_s: float, verbose: bool = True):
    print(f"WAIT att=1 timeout={timeout_s:.1f}s", flush=True)
    records = []
    end = time.monotonic() + timeout_s
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
            print("!!! ESTOP while waiting attitude", flush=True)
            return records, False
        if tel.get("att", 0) == 1:
            return records, True
    return records, False


def parse_targets(text: str):
    targets = []
    for item in text.split(","):
        item = item.strip()
        if item:
            targets.append(int(item))
    return targets


def continuous_settle_ms(samples, err_limit_deg10: int, window_ms: int):
    if not samples:
        return None
    for index, sample in enumerate(samples):
        start_t = sample.get("t", 0)
        end_t = start_t + window_ms
        window = [r for r in samples[index:] if r.get("t", 0) <= end_t]
        if not window or window[-1].get("t", 0) < end_t:
            continue
        if all(abs(r.get("err", 0)) <= err_limit_deg10 for r in window):
            return start_t - samples[0].get("t", 0)
    return None


def first_enter_ms(samples, err_limit_deg10: float):
    if not samples:
        return None
    t0 = samples[0].get("t", 0)
    for record in samples:
        if abs(record.get("err", 0)) <= err_limit_deg10:
            return record.get("t", 0) - t0
    return None


def overshoot_deg10(samples):
    if len(samples) < 2:
        return 0
    initial_err = samples[0].get("err", 0)
    if abs(initial_err) <= 20:
        return 0
    if initial_err > 0:
        crossed = [-r.get("err", 0) for r in samples if r.get("err", 0) < 0]
    else:
        crossed = [r.get("err", 0) for r in samples if r.get("err", 0) > 0]
    return max(crossed) if crossed else 0


def zero_crossings(values):
    count = 0
    last = 0
    for value in values:
        sign = 1 if value > 0 else (-1 if value < 0 else 0)
        if sign == 0:
            continue
        if last != 0 and sign != last:
            count += 1
        last = sign
    return count


def analyze_yaw_segment(name: str, records: list[dict], target_deg10: int, out_limit: int):
    samples = [
        r for r in records
        if r.get("tgt") == target_deg10 and r.get("att", 0) == 1 and not r.get("estop", 0)
    ]
    if len(samples) < 5:
        print(f"ANALYZE {name}: insufficient samples n={len(samples)}")
        return None

    t0 = samples[0].get("t", 0)
    active = [r for r in samples if r.get("t", 0) - t0 >= 300] or samples
    tail = active[-min(20, len(active)):]

    err_values = [r.get("err", 0) for r in active]
    yaw_tail = [r.get("yaw", 0) for r in tail]
    err_tail = [r.get("err", 0) for r in tail]
    turn_values = [r.get("turn", 0) for r in active]

    initial_abs_err = abs(active[0].get("err", 0))
    t90 = first_enter_ms(active, initial_abs_err * 0.1) if initial_abs_err > 20 else 0
    settle_2 = continuous_settle_ms(active, 20, 1000)
    settle_5 = continuous_settle_ms(active, 50, 1000)
    overshoot = overshoot_deg10(active)
    max_abs_err = max(abs(v) for v in err_values)
    max_abs_turn = max(abs(v) for v in turn_values)
    sat_ratio = 0.0
    if out_limit > 0:
        sat_ratio = sum(1 for v in turn_values if abs(v) >= int(out_limit * 0.95)) / len(turn_values)
    tail_std = pstdev(err_tail) / 10.0 if len(err_tail) >= 2 else 0.0
    crossings = zero_crossings(err_values)

    result = {
        "name": name,
        "target_deg": target_deg10 / 10.0,
        "final_yaw_deg": mean(yaw_tail) / 10.0,
        "final_err_deg": mean(err_tail) / 10.0,
        "tail_std_deg": tail_std,
        "max_abs_err_deg": max_abs_err / 10.0,
        "overshoot_deg": overshoot / 10.0,
        "t90_ms": t90,
        "settle_2deg_ms": settle_2,
        "settle_5deg_ms": settle_5,
        "max_abs_turn_rpm": max_abs_turn,
        "sat_ratio": sat_ratio,
        "zero_crossings": crossings,
        "n": len(active),
    }

    print(
        "ANALYZE {name} target={target_deg:.1f}deg final_yaw={final_yaw_deg:.1f}deg "
        "final_err={final_err_deg:.1f}deg tail_std={tail_std_deg:.2f}deg "
        "overshoot={overshoot_deg:.1f}deg t90_ms={t90_ms} "
        "settle2_ms={settle_2deg_ms} settle5_ms={settle_5deg_ms} "
        "max_turn={max_abs_turn_rpm} sat={sat_ratio:.0%} zero_cross={zero_crossings} n={n}".format(**result)
    )
    return result


def print_summary(results):
    valid = [r for r in results if r is not None]
    if not valid:
        return
    worst_static = max(valid, key=lambda r: abs(r["final_err_deg"]))
    worst_over = max(valid, key=lambda r: r["overshoot_deg"])
    slow = [r for r in valid if r["settle_2deg_ms"] is None]
    saturated = [r for r in valid if r["sat_ratio"] > 0.2]

    print("SUMMARY")
    print(f"  worst_static: {worst_static['name']} err={worst_static['final_err_deg']:.1f}deg")
    print(f"  worst_overshoot: {worst_over['name']} over={worst_over['overshoot_deg']:.1f}deg")
    if slow:
        print("  not_settled_2deg: " + ", ".join(r["name"] for r in slow))
    if saturated:
        print("  output_saturated: " + ", ".join(f"{r['name']}({r['sat_ratio']:.0%})" for r in saturated))


def main():
    parser = argparse.ArgumentParser(description="Yaw PID serial tuning helper")
    parser.add_argument("--port", default="COM19")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--base", type=int, default=0, help="base speed rpm")
    parser.add_argument("--kp", type=int, default=95, help="yaw Kp milli")
    parser.add_argument("--ki", type=int, default=2, help="yaw Ki milli")
    parser.add_argument("--kd", type=int, default=45, help="yaw Kd milli")
    parser.add_argument("--out", type=int, default=160, help="yaw output limit rpm")
    parser.add_argument("--min-turn", type=int, default=14, help="minimum yaw turn rpm feed-forward")
    parser.add_argument("--deadband", type=int, default=15, help="yaw deadband deg10")
    parser.add_argument("--izone", type=int, default=300, help="integral zone deg10")
    parser.add_argument("--ilim", type=int, default=20, help="integral contribution limit rpm")
    parser.add_argument("--hold", type=float, default=6.0, help="seconds per yaw target")
    parser.add_argument("--targets", default="0,450,900,1350,1800,0")
    parser.add_argument("--wait-att", type=float, default=45.0)
    parser.add_argument("--quiet", action="store_true", help="suppress raw TEL lines")
    args = parser.parse_args()

    verbose = not args.quiet
    plan = parse_targets(args.targets)
    segments = []

    with serial.Serial(args.port, args.baud, timeout=0.2) as ser:
        print(f"OPEN {args.port} {args.baud}")
        time.sleep(0.5)
        ser.reset_input_buffer()

        setup_cmds = [
            "CLR",
            f"PIDY {args.kp} {args.ki} {args.kd}",
            f"OUTY {args.out}",
            f"MINY {args.min_turn}",
            f"DBY {args.deadband}",
            f"IZONEY {args.izone}",
            f"ILIMY {args.ilim}",
            f"BASE {args.base}",
            "START",
        ]
        for cmd in setup_cmds:
            send(ser, cmd)
            collect(ser, 0.35 if cmd not in {"CLR", "START"} else 0.8, verbose)

        _, ready = wait_attitude_ready(ser, args.wait_att, verbose)
        if not ready:
            print("ERROR: attitude not ready, abort", flush=True)
            send(ser, "STOP")
            return 3

        ok = True
        for target in plan:
            name = f"yaw_{target / 10:.1f}deg"
            send(ser, f"YAW10 {target}")
            records, ok = collect(ser, args.hold, verbose)
            segments.append((name, records, target))
            if not ok:
                break

        send(ser, "STOP")
        collect(ser, 1.0, verbose)

    results = [analyze_yaw_segment(name, records, target, args.out) for name, records, target in segments]
    print_summary(results)
    return 0 if ok else 2


if __name__ == "__main__":
    raise SystemExit(main())
