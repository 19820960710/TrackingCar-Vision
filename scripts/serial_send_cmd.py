#!/usr/bin/env python3
"""向调试串口发送单条或多条命令，用于在线调参。"""
import argparse
import time
import serial

ap = argparse.ArgumentParser()
ap.add_argument("--port", default="COM19")
ap.add_argument("--baud", type=int, default=115200)
ap.add_argument("--read-ms", type=int, default=0, help="发完每条后读若干 ms 回显")
ap.add_argument("cmd", nargs="+")
args = ap.parse_args()

with serial.Serial(args.port, args.baud, timeout=0.2, write_timeout=1.0) as ser:
    time.sleep(0.2)
    ser.reset_input_buffer()
    for cmd in args.cmd:
        ser.write((cmd + "\r\n").encode("ascii"))
        ser.flush()
        print(f">>> {cmd}")
        if args.read_ms > 0:
            end = time.monotonic() + args.read_ms / 1000.0
            while time.monotonic() < end:
                raw = ser.readline()
                if not raw:
                    continue
                text = raw.decode("utf-8", errors="replace").strip()
                if text:
                    print(text.encode("ascii", errors="replace").decode("ascii"), flush=True)
                end = time.monotonic() + 0.02
