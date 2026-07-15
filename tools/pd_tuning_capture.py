#!/usr/bin/env python3
"""Capture and score MSPM0 gimbal PD auto-tuning trials."""

from __future__ import annotations

import argparse
import csv
import json
import time
from dataclasses import asdict, dataclass
from datetime import datetime
from pathlib import Path

import serial


PD_FIELDS = (
    "mcu_ms",
    "state",
    "trial",
    "valid",
    "dx",
    "dy",
    "yaw_p_milli",
    "yaw_d_milli",
    "yaw_output",
    "pitch_p_milli",
    "pitch_d_milli",
    "pitch_output",
)


@dataclass
class TrialScore:
    trial: int
    axis: str
    direction: str
    sample_count: int
    initial_error_pixels: int | None
    final_error_pixels: int | None
    peak_abs_error_pixels: int | None
    overshoot_pixels: int | None
    overshoot_ratio: float | None
    settle_time_seconds: float | None
    iae_pixel_seconds: float | None
    zero_crossings: int
    invalid_samples: int


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--port", default="COM11")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--duration", type=float, default=60.0)
    parser.add_argument("--settle-band", type=int, default=4)
    parser.add_argument("--settle-hold", type=float, default=0.5)
    parser.add_argument(
        "--output-root",
        type=Path,
        default=Path(__file__).resolve().parent / "pd_tuning_logs",
    )
    parser.add_argument("--label", default="baseline")
    parser.add_argument(
        "--wait-for-start",
        action="store_true",
        help="Ignore an old completed run until state=0, trial=0 is observed.",
    )
    return parser.parse_args()


def parse_pd_line(line: str, host_elapsed: float) -> dict[str, int | float] | None:
    parts = line.split(",")
    if len(parts) != 13 or parts[0] != "PD":
        return None
    try:
        values = [int(value) for value in parts[1:]]
    except ValueError:
        return None
    row: dict[str, int | float] = {"host_elapsed": host_elapsed}
    row.update(dict(zip(PD_FIELDS, values, strict=True)))
    return row


def find_settle_time(
    rows: list[dict[str, int | float]], band: int, hold_seconds: float
) -> float | None:
    if not rows:
        return None
    axis_key = "dx" if int(rows[0]["trial"]) < 2 else "dy"
    start_ms = int(rows[0]["mcu_ms"])
    hold_ms = int(hold_seconds * 1000.0)
    for index, row in enumerate(rows):
        if abs(int(row[axis_key])) > band:
            continue
        candidate_ms = int(row["mcu_ms"])
        held = True
        reached_hold = False
        for later in rows[index:]:
            if abs(int(later[axis_key])) > band:
                held = False
                break
            if int(later["mcu_ms"]) - candidate_ms >= hold_ms:
                reached_hold = True
                break
        if held and reached_hold:
            return (candidate_ms - start_ms) / 1000.0
    return None


def count_zero_crossings(errors: list[int]) -> int:
    crossings = 0
    previous_sign = 0
    for error in errors:
        sign = 1 if error > 0 else (-1 if error < 0 else 0)
        if sign == 0:
            continue
        if previous_sign != 0 and sign != previous_sign:
            crossings += 1
        previous_sign = sign
    return crossings


def score_trial(
    all_rows: list[dict[str, int | float]], trial: int, band: int, hold: float
) -> TrialScore:
    rows = [
        row
        for row in all_rows
        if int(row["trial"]) == trial and int(row["state"]) in (2, 3)
    ]
    axis = "yaw" if trial < 2 else "pitch"
    direction = "positive" if (trial & 1) == 0 else "negative"
    if not rows:
        return TrialScore(
            trial, axis, direction, 0, None, None, None, None, None, None,
            None, 0, 0
        )

    axis_key = "dx" if axis == "yaw" else "dy"
    errors = [int(row[axis_key]) for row in rows]
    initial = errors[0]
    opposite = [error for error in errors if error * initial < 0]
    overshoot = max((abs(error) for error in opposite), default=0)
    iae = 0.0
    for previous, current in zip(rows, rows[1:]):
        dt = max(0, int(current["mcu_ms"]) - int(previous["mcu_ms"])) / 1000.0
        iae += (abs(int(previous[axis_key])) + abs(int(current[axis_key]))) * 0.5 * dt

    return TrialScore(
        trial=trial,
        axis=axis,
        direction=direction,
        sample_count=len(rows),
        initial_error_pixels=initial,
        final_error_pixels=errors[-1],
        peak_abs_error_pixels=max(abs(error) for error in errors),
        overshoot_pixels=overshoot,
        overshoot_ratio=(overshoot / abs(initial)) if initial != 0 else None,
        settle_time_seconds=find_settle_time(rows, band, hold),
        iae_pixel_seconds=round(iae, 4),
        zero_crossings=count_zero_crossings(errors),
        invalid_samples=sum(int(row["valid"]) == 0 for row in rows),
    )


def main() -> int:
    args = parse_args()
    stamp = datetime.now().strftime("%Y%m%d-%H%M%S")
    run_dir = args.output_root / f"{stamp}_{args.label}"
    run_dir.mkdir(parents=True, exist_ok=False)
    raw_path = run_dir / "serial_raw.log"
    csv_path = run_dir / "pd_samples.csv"
    summary_path = run_dir / "summary.json"

    rows: list[dict[str, int | float]] = []
    raw_records: list[tuple[float, str]] = []
    started = time.monotonic()
    completed_at: float | None = None
    run_started = not args.wait_for_start
    with serial.Serial(args.port, args.baud, timeout=0.05) as uart:
        buffer = bytearray()
        while time.monotonic() - started < args.duration:
            chunk = uart.read(uart.in_waiting or 1)
            if chunk:
                buffer.extend(chunk)
            while b"\n" in buffer:
                raw, _, buffer = buffer.partition(b"\n")
                line = raw.rstrip(b"\r").decode("ascii", errors="replace")
                elapsed = time.monotonic() - started
                if not line:
                    continue
                raw_records.append((elapsed, line))
                row = parse_pd_line(line, elapsed)
                if row is not None:
                    if not run_started:
                        if int(row["state"]) != 0 or int(row["trial"]) != 0:
                            continue
                        run_started = True
                    rows.append(row)
                    state = int(row["state"])
                    if state in (4, 5) and completed_at is None:
                        completed_at = elapsed
            if completed_at is not None and time.monotonic() - started >= completed_at + 1.0:
                break

    with raw_path.open("w", encoding="utf-8", newline="\n") as stream:
        for elapsed, line in raw_records:
            stream.write(f"{elapsed:.6f} {line}\n")

    csv_fields = ("host_elapsed",) + PD_FIELDS
    with csv_path.open("w", encoding="utf-8", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=csv_fields)
        writer.writeheader()
        writer.writerows(rows)

    final_state = int(rows[-1]["state"]) if rows else None
    summary = {
        "port": args.port,
        "baud": args.baud,
        "label": args.label,
        "duration_seconds": round(time.monotonic() - started, 3),
        "raw_line_count": len(raw_records),
        "pd_sample_count": len(rows),
        "sample_rate_hz": round(len(rows) / max(time.monotonic() - started, 0.001), 3),
        "final_state": final_state,
        "completed": final_state == 4,
        "aborted": final_state == 5,
        "trials": [
            asdict(score_trial(rows, trial, args.settle_band, args.settle_hold))
            for trial in range(4)
        ],
    }
    summary_path.write_text(json.dumps(summary, indent=2), encoding="utf-8")
    print(json.dumps(summary, indent=2))
    print(f"run_dir={run_dir}")
    return 0 if rows else 2


if __name__ == "__main__":
    raise SystemExit(main())
