#!/usr/bin/env python3
"""采集一段 TEL 并打印 err/turn/wl/wr/l/r/lp/rp 逐行，便于看回正过程。"""
import argparse
import re
import time
import serial

TEL_RE = re.compile(r"TEL\s+(.*)")
PAIR_RE = re.compile(r"(\w+)=(-?\d+)")


def parse_tel(line):
    m = TEL_RE.search(line)
    if not m:
        return None
    data = {k: int(v) for k, v in PAIR_RE.findall(m.group(1))}
    return data if {"t", "att", "err", "turn"}.issubset(data) else None


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", default="COM19")
    ap.add_argument("--baud", type=int, default=115200)
    ap.add_argument("--hold", type=float, default=20.0)
    args = ap.parse_args()

    with serial.Serial(args.port, args.baud, timeout=0.2) as ser:
        time.sleep(0.3)
        ser.reset_input_buffer()

        # 等待姿态有效，避免 att!=1 时采集到空数据
        print("WAIT att=1 ...", flush=True)
        att_end = time.monotonic() + 90
        att_ready = False
        while time.monotonic() < att_end:
            raw = ser.readline()
            if not raw:
                continue
            tel = parse_tel(raw.decode("utf-8", errors="replace").strip())
            if tel is None:
                continue
            if tel.get("att", 0) == 1:
                att_ready = True
                break
        if not att_ready:
            print("ERROR: attitude not ready", flush=True)
            return 3
        print("att=1, start trace", flush=True)

        end = time.monotonic() + args.hold
        while time.monotonic() < end:
            raw = ser.readline()
            if not raw:
                continue
            tel = parse_tel(raw.decode("utf-8", errors="replace").strip())
            if tel is None:
                continue
            if tel.get("att", 0) != 1:
                continue
            keys = ["t", "err", "ce", "derr", "boost", "turn",
                    "wl", "wr", "ls", "rs", "l", "r", "lp", "rp"]
            print(" ".join(f"{k}={tel.get(k, 0)}" for k in keys), flush=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
