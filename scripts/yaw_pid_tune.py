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


def collect(ser, duration_s: float, verbose: bool = True):
    records = []
    end = time.monotonic() + duration_s
    while time.monotonic() < end:
        raw = ser.readline()
        if not raw:
            continue
        text = raw.decode("utf-8", errors="replace").strip()
        if text and verbose:
            print(text.encode("ascii", errors="replace").decode("ascii"), flush=True)
        tel = parse_tel(text)
        if tel:
            records.append(tel)
            if tel.get("estop", 0):
                print("!!! ESTOP detected", flush=True)
                send(ser, "STOP")
                return records, False
    return records, True


def wait_attitude_ready(ser, timeout_s: float, verbose: bool = True):
    print(f"WAIT att=1 timeout={timeout_s}s", flush=True)
    end = time.monotonic() + timeout_s
    records = []
    while time.monotonic() < end:
        raw = ser.readline()
        if not raw:
            continue
        text = raw.decode("utf-8", errors="replace").strip()
        if text and verbose:
            print(text.encode("ascii", errors="replace").decode("ascii"), flush=True)
        tel = parse_tel(text)
        if tel:
            records.append(tel)
            if tel.get("estop", 0):
                return records, False
            if tel.get("att", 0) == 1:
                return records, True
    return records, False


def analyze(name, records, target_deg10):
    samples = [
        r for r in records
        if r.get("tgt") == target_deg10 and r.get("att", 0) == 1 and not r.get("estop", 0)
    ]
    if len(samples) < 5:
        print(f"ANALYZE {name}: insufficient samples n={len(samples)}")
        return

    t0 = samples[0].get("t", 0)
    active = [r for r in samples if r.get("t", 0) - t0 >= 300] or samples
    tail = active[-min(10, len(active)):]
    yaw_tail = [r.get("yaw", 0) for r in tail]
    err_tail = [r.get("err", 0) for r in tail]
    turn_vals = [r.get("turn", 0) for r in active]

    final_yaw = mean(yaw_tail) / 10.0
    final_err = mean(err_tail) / 10.0
    max_abs_err = max(abs(r.get("err", 0)) for r in active) / 10.0
    max_abs_turn = max(abs(v) for v in turn_vals)
    settle = next((r.get("t", 0) - t0 for r in active if abs(r.get("err", 0)) <= 20), None)

    print(
        f"ANALYZE {name} target={target_deg10/10:.1f}deg "
        f"final_yaw={final_yaw:.1f}deg final_err={final_err:.1f}deg "
        f"max_abs_err={max_abs_err:.1f}deg max_abs_turn={max_abs_turn}rpm "
        f"settle_2deg_ms={settle} n={len(active)}"
    )


def main():
    ap = argparse.ArgumentParser(description="Yaw PID serial tuning helper")
    ap.add_argument("--port", default="COM19")
    ap.add_argument("--baud", type=int, default=115200)
    ap.add_argument("--base", type=int, default=0, help="base speed rpm")
    ap.add_argument("--kp", type=int, default=95, help="yaw Kp milli")
    ap.add_argument("--ki", type=int, default=2, help="yaw Ki milli")
    ap.add_argument("--kd", type=int, default=45, help="yaw Kd milli")
    ap.add_argument("--out", type=int, default=160, help="yaw output limit rpm")
    ap.add_argument("--min-turn", type=int, default=14, help="minimum yaw turn rpm feed-forward")
    ap.add_argument("--deadband", type=int, default=15, help="yaw deadband deg10")
    ap.add_argument("--izone", type=int, default=300, help="integral zone deg10")
    ap.add_argument("--ilim", type=int, default=20, help="integral contribution limit rpm")
    ap.add_argument("--hold", type=float, default=5.0, help="seconds per yaw target")
    ap.add_argument("--targets", default="0,450,900,1350,1800,0",
                    help="comma separated yaw targets in deg10, e.g. 0 or 0,450,900")
    ap.add_argument("--quiet", action="store_true", help="suppress raw TEL lines")
    args = ap.parse_args()
    verbose = not args.quiet

    plan = [int(item.strip()) for item in args.targets.split(",") if item.strip()]
    segments = []

    with serial.Serial(args.port, args.baud, timeout=0.2) as ser:
        print(f"OPEN {args.port} {args.baud}")
        time.sleep(0.5)
        ser.reset_input_buffer()

        send(ser, "CLR")
        collect(ser, 0.8, verbose)
        send(ser, f"PIDY {args.kp} {args.ki} {args.kd}")
        collect(ser, 0.5, verbose)
        send(ser, f"OUTY {args.out}")
        collect(ser, 0.5, verbose)
        send(ser, f"MINY {args.min_turn}")
        collect(ser, 0.3, verbose)
        send(ser, f"DBY {args.deadband}")
        collect(ser, 0.3, verbose)
        send(ser, f"IZONEY {args.izone}")
        collect(ser, 0.3, verbose)
        send(ser, f"ILIMY {args.ilim}")
        collect(ser, 0.3, verbose)
        send(ser, f"BASE {args.base}")
        collect(ser, 0.8, verbose)
        send(ser, "START")
        collect(ser, 0.8, verbose)
        _, ready = wait_attitude_ready(ser, 45.0, verbose)
        if not ready:
            print("ERROR: attitude not ready, abort", flush=True)
            send(ser, "STOP")
            return 3

        ok = True
        for target in plan:
            name = f"yaw_{target // 10}deg"
            send(ser, f"YAW10 {target}")
            records, ok = collect(ser, args.hold, verbose)
            segments.append((name, records, target))
            if not ok:
                break

        send(ser, "STOP")
        collect(ser, 1.0, verbose)

    for seg in segments:
        analyze(*seg)

    return 0 if ok else 2


if __name__ == "__main__":
    sys.exit(main())
