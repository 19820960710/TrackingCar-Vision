#!/usr/bin/env python3
"""
0° 保持态扰动回正采集器。

流程：发 CLR+参数+START+YAW10 0，进入保持态后持续采集 TEL。
你在此期间手动扰动车身。脚本把每次“误差从大回到小”的过程切片，
打印回正曲线关键点，判断是否分段停顿。

用法：
  .venv/Scripts/python.exe scripts/yaw_disturb_collect.py --port COM19 --hold 25
"""
import argparse
import re
import sys
import time

try:
    import serial
except ImportError:
    print("缺少 pyserial", file=sys.stderr)
    raise

TEL_RE = re.compile(r"TEL\s+(.*)")
PAIR_RE = re.compile(r"(\w+)=(-?\d+)")


def parse_tel(line: str):
    m = TEL_RE.search(line)
    if not m:
        return None
    data = {k: int(v) for k, v in PAIR_RE.findall(m.group(1))}
    required = {"seq", "t", "att", "tgt", "yaw", "err", "turn"}
    return data if required.issubset(data) else None


def send(ser, cmd):
    ser.write((cmd + "\r\n").encode("ascii"))
    ser.flush()
    print(f">>> {cmd}", flush=True)


def collect(ser, duration_s):
    records = []
    end = time.monotonic() + duration_s
    while time.monotonic() < end:
        raw = ser.readline()
        if not raw:
            continue
        tel = parse_tel(raw.decode("utf-8", errors="replace").strip())
        if tel is None:
            continue
        records.append(tel)
        if tel.get("estop", 0):
            print("!!! ESTOP", flush=True)
            send(ser, "STOP")
            return records, False
    return records, True


def split_events(records):
    """把采集切成若干“扰动事件”：误差峰值 -> 回到接近 0。"""
    if len(records) < 3:
        return []
    events = []
    in_event = False
    peak_abs = 0
    peak_idx = 0
    start_idx = 0

    for i, r in enumerate(records):
        abs_err = abs(r.get("err", 0))
        if abs_err >= 30:  # 视为一次扰动开始
            if not in_event:
                in_event = True
                start_idx = i
                peak_abs = abs_err
                peak_idx = i
            elif abs_err > peak_abs:
                peak_abs = abs_err
                peak_idx = i
        elif in_event and abs_err <= 20:
            events.append((start_idx, peak_idx, i, records[start_idx:i + 1]))
            in_event = False

    if in_event:
        events.append((start_idx, peak_idx, len(records) - 1, records[start_idx:]))
    return events


def analyze_event(name, seg):
    if len(seg) < 5:
        print(f"{name}: samples too few n={len(seg)}")
        return
    t0 = seg[0].get("t", 0)
    peak = max(seg, key=lambda r: abs(r.get("err", 0)))
    peak_t = peak.get("t", 0) - t0
    peak_err = peak.get("err", 0)

    # 找回正过程中几个过点
    thresholds = [20, 10, 5, 2]
    reach = {}
    for th in thresholds:
        for r in seg:
            if abs(r.get("err", 0)) <= th:
                reach[th] = r.get("t", 0) - t0
                break

    # 检测“停顿”：连续 >= 400ms 误差变化 < 1°
    stalls = 0
    last_t = seg[0].get("t", 0)
    last_err = seg[0].get("err", 0)
    stall_start = None
    for r in seg:
        dt = r.get("t", 0) - last_t
        derr = abs(r.get("err", 0) - last_err)
        if dt >= 100 and derr <= 10:
            if stall_start is None:
                stall_start = last_t
            elif r.get("t", 0) - stall_start >= 400:
                stalls += 1
                stall_start = r.get("t", 0)
        else:
            stall_start = None
        last_t = r.get("t", 0)
        last_err = r.get("err", 0)

    print(
        f"{name}: peak={peak_err/10:.1f}deg@{peak_t}ms "
        f"reach20={reach.get(20)} reach10={reach.get(10)} "
        f"reach5={reach.get(5)} reach2={reach.get(2)} stalls={stalls} n={len(seg)}"
    )


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", default="COM19")
    ap.add_argument("--baud", type=int, default=115200)
    ap.add_argument("--kp", type=int, default=80)
    ap.add_argument("--ki", type=int, default=1)
    ap.add_argument("--kd", type=int, default=60)
    ap.add_argument("--out", type=int, default=160)
    ap.add_argument("--min-turn", type=int, default=15)
    ap.add_argument("--deadband", type=int, default=15)
    ap.add_argument("--izone", type=int, default=180)
    ap.add_argument("--ilim", type=int, default=6)
    ap.add_argument("--zone", type=int, default=180)
    ap.add_argument("--ramp", type=int, default=80)
    ap.add_argument("--ffs", type=int, default=None)
    ap.add_argument("--hold", type=float, default=25.0)
    ap.add_argument("--wait-att", type=float, default=90.0)
    args = ap.parse_args()

    with serial.Serial(args.port, args.baud, timeout=0.2) as ser:
        print(f"OPEN {args.port}")
        time.sleep(0.5)
        ser.reset_input_buffer()
        send(ser, "CLR")
        collect(ser, 0.8)
        for cmd in [
            f"PIDY {args.kp} {args.ki} {args.kd}",
            f"OUTY {args.out}",
            f"MINY {args.min_turn}",
            f"DBY {args.deadband}",
            f"IZONEY {args.izone}",
            f"ILIMY {args.ilim}",
            f"ZONEY {args.zone}",
            f"RAMPY {args.ramp}",
            *([f"FFS {args.ffs}"] if args.ffs is not None else []),
            "BASE 0",
            "START",
        ]:
            send(ser, cmd)
            collect(ser, 0.35)

        print(f"WAIT att=1 timeout={args.wait_att:.0f}s", flush=True)
        end = time.monotonic() + args.wait_att
        ready = False
        while time.monotonic() < end:
            raw = ser.readline()
            if not raw:
                continue
            tel = parse_tel(raw.decode("utf-8", errors="replace").strip())
            if tel and tel.get("att", 0) == 1:
                ready = True
                break
            if tel and tel.get("estop", 0):
                print("!!! ESTOP while waiting", flush=True)
                send(ser, "STOP")
                return 3
        if not ready:
            print("ERROR: attitude not ready", flush=True)
            send(ser, "STOP")
            return 3

        send(ser, "YAW10 0")
        print(">>> 开始扰动测试，请手动扰动车身 <<<", flush=True)
        records, ok = collect(ser, args.hold)
        send(ser, "STOP")
        collect(ser, 1.0)

    if not ok:
        return 2

    events = split_events(records)
    if not events:
        print("未检测到明显扰动事件（需要误差峰值 >= 3°）")
        print(f"总样本 n={len(records)}")
    else:
        print(f"检测到 {len(events)} 个扰动事件：")
        for idx, (s, p, e, seg) in enumerate(events):
            analyze_event(f"event_{idx}", seg)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
