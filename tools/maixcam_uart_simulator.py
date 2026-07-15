#!/usr/bin/env python3
"""Feed repeatable MaixCAM AIM frames and capture MSPM0 diagnostics."""

from __future__ import annotations

import argparse
import csv
import json
import math
import sys
import time
from dataclasses import asdict, dataclass, field
from datetime import datetime
from pathlib import Path

try:
    import serial
    from serial.tools import list_ports
except ImportError as exc:
    raise SystemExit("pyserial is required: python -m pip install pyserial") from exc


FRAME_WIDTH = 512
FRAME_HEIGHT = 320
DEFAULT_PORT = "COM11"
DEFAULT_BAUD = 115200
DEFAULT_FPS = 100.0
DEFAULT_READY_TIMEOUT_S = 15.0
FINAL_CENTER_DURATION_S = 1.0
DEFAULT_TRAJECTORY_DURATION_S = 16.0
DEFAULT_TRAJECTORY_PERIOD_S = 8.0
DEFAULT_YAW_AMPLITUDE_PIXELS = 120
DEFAULT_PITCH_AMPLITUDE_PIXELS = 70
DEFAULT_TRAJECTORY_RAMP_S = 1.0
COUNTER_NAMES = ("Q", "J", "F", "T", "A", "R", "P", "E")


@dataclass(frozen=True)
class Scenario:
    name: str
    dx: int
    dy: int
    duration_s: float


@dataclass(frozen=True)
class TrajectorySample:
    index: int
    time_s: float
    dx: int
    dy: int


@dataclass
class TrajectoryResult:
    cycle: int
    name: str
    duration_s: float
    sample_count: int
    tv_lines: int = 0
    pitch_submit_count: int = 0
    yaw_submit_count: int = 0
    counter_start: dict[str, dict[str, int]] = field(default_factory=dict)
    counter_end: dict[str, dict[str, int]] = field(default_factory=dict)
    counter_delta: dict[str, dict[str, int]] = field(default_factory=dict)


SCENARIOS = (
    Scenario("CENTER", 0, 0, 1.0),
    Scenario("YAW_POS", 40, 0, 2.0),
    Scenario("CENTER", 0, 0, 1.0),
    Scenario("YAW_NEG", -40, 0, 2.0),
    Scenario("CENTER", 0, 0, 1.0),
    Scenario("PITCH_POS", 0, 40, 2.0),
    Scenario("CENTER", 0, 0, 1.0),
    Scenario("PITCH_NEG", 0, -40, 2.0),
    Scenario("CENTER", 0, 0, 1.0),
    Scenario("DUAL_POS", 40, 40, 2.0),
    Scenario("CENTER", 0, 0, 1.0),
    Scenario("DUAL_NEG", -40, -40, 2.0),
    Scenario("CENTER", 0, 0, 2.0),
)


@dataclass
class ScenarioResult:
    cycle: int
    name: str
    dx: int
    dy: int
    duration_s: float
    tv_lines: int = 0
    pitch_submit_count: int = 0
    yaw_submit_count: int = 0
    counter_start: dict[str, dict[str, int]] = field(default_factory=dict)
    counter_end: dict[str, dict[str, int]] = field(default_factory=dict)
    counter_delta: dict[str, dict[str, int]] = field(default_factory=dict)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Send AIM frames, wait for VISION_READY, and record TV/MS lines."
    )
    parser.add_argument("--port", default=DEFAULT_PORT)
    parser.add_argument("--baud", type=int, default=DEFAULT_BAUD)
    parser.add_argument("--fps", type=float, default=DEFAULT_FPS)
    parser.add_argument("--cycles", type=int, default=1)
    parser.add_argument(
        "--mode",
        choices=("scenarios", "trajectory"),
        default="scenarios",
        help="Use fixed diagnostic scenarios or a continuous elliptical path.",
    )
    parser.add_argument(
        "--trajectory-duration",
        type=float,
        default=DEFAULT_TRAJECTORY_DURATION_S,
        help="Duration of one continuous trajectory cycle in seconds.",
    )
    parser.add_argument(
        "--trajectory-period",
        type=float,
        default=DEFAULT_TRAJECTORY_PERIOD_S,
        help="Time for one ellipse revolution in seconds.",
    )
    parser.add_argument(
        "--yaw-amplitude",
        type=int,
        default=DEFAULT_YAW_AMPLITUDE_PIXELS,
        help="Maximum absolute horizontal error in pixels.",
    )
    parser.add_argument(
        "--pitch-amplitude",
        type=int,
        default=DEFAULT_PITCH_AMPLITUDE_PIXELS,
        help="Maximum absolute vertical error in pixels.",
    )
    parser.add_argument(
        "--trajectory-ramp",
        type=float,
        default=DEFAULT_TRAJECTORY_RAMP_S,
        help="Smooth center-to-ellipse ramp time at both ends.",
    )
    parser.add_argument(
        "--generate-only",
        type=Path,
        help="Generate the trajectory CSV and exit without opening a serial port.",
    )
    parser.add_argument(
        "--ready-timeout",
        type=float,
        default=DEFAULT_READY_TIMEOUT_S,
    )
    parser.add_argument(
        "--pre-send-delay",
        type=float,
        default=0.0,
        help="After VISION_READY, listen without transmitting for this many seconds.",
    )
    parser.add_argument(
        "--log-dir",
        type=Path,
        default=Path(__file__).resolve().parent / "test_logs",
    )
    parser.add_argument(
        "--list-ports",
        action="store_true",
        help="List serial ports and exit.",
    )
    return parser.parse_args()


def aim_frame(dx: int, dy: int) -> bytes:
    target_x = max(0, min(FRAME_WIDTH - 1, FRAME_WIDTH // 2 + dx))
    target_y = max(0, min(FRAME_HEIGHT - 1, FRAME_HEIGHT // 2 + dy))
    return (
        f"AIM,0,0,0,{target_x},{target_y},0,0,"
        "blob-fallback,NO_LASER\n"
    ).encode("ascii")


def smoothstep(value: float) -> float:
    value = max(0.0, min(1.0, value))
    return value * value * (3.0 - 2.0 * value)


def generate_continuous_error_array(
    fps: float,
    duration_s: float,
    period_s: float,
    yaw_amplitude: int,
    pitch_amplitude: int,
    ramp_s: float,
) -> list[TrajectorySample]:
    sample_count = max(2, int(round(duration_s * fps)))
    samples: list[TrajectorySample] = []

    for index in range(sample_count):
        time_s = index / fps
        phase = 2.0 * math.pi * time_s / period_s
        if ramp_s > 0.0:
            ramp_fraction = min(
                1.0,
                time_s / ramp_s,
                max(0.0, duration_s - time_s) / ramp_s,
            )
            envelope = smoothstep(ramp_fraction)
        else:
            envelope = 1.0
        dx = int(round(yaw_amplitude * envelope * math.sin(phase)))
        dy = int(round(pitch_amplitude * envelope * math.cos(phase)))
        samples.append(TrajectorySample(index, time_s, dx, dy))

    return samples


def write_trajectory_csv(path: Path, samples: list[TrajectorySample]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8", newline="") as output:
        writer = csv.writer(output, lineterminator="\n")
        writer.writerow(("index", "time_s", "dx", "dy", "target_x", "target_y"))
        for sample in samples:
            writer.writerow(
                (
                    sample.index,
                    f"{sample.time_s:.6f}",
                    sample.dx,
                    sample.dy,
                    FRAME_WIDTH // 2 + sample.dx,
                    FRAME_HEIGHT // 2 + sample.dy,
                )
            )


def copy_counters(
    counters: dict[str, dict[str, int]],
) -> dict[str, dict[str, int]]:
    return {axis: values.copy() for axis, values in counters.items()}


def counter_delta(
    start: dict[str, dict[str, int]],
    end: dict[str, dict[str, int]],
) -> dict[str, dict[str, int]]:
    result: dict[str, dict[str, int]] = {}
    for axis in sorted(set(start) | set(end)):
        result[axis] = {
            name: end.get(axis, {}).get(name, 0)
            - start.get(axis, {}).get(name, 0)
            for name in COUNTER_NAMES
        }
    return result


class Capture:
    def __init__(self, raw_log) -> None:
        self.raw_log = raw_log
        self.started_s = time.perf_counter()
        self.rx_buffer = bytearray()
        self.ready = False
        self.fault: str | None = None
        self.protocol_error_observed = False
        self.protocol_error_counts: dict[str, int] = {}
        self.current_result: ScenarioResult | TrajectoryResult | None = None
        self.motor_counters: dict[str, dict[str, int]] = {}
        self.tv_line_count = 0
        self.ms_line_count = 0
        self.unknown_line_count = 0

    def log(self, kind: str, text: str) -> None:
        elapsed_s = time.perf_counter() - self.started_s
        output = f"{elapsed_s:9.3f} {kind} {text}"
        print(output, flush=True)
        self.raw_log.write(output + "\n")
        self.raw_log.flush()

    def consume_bytes(self, received: bytes) -> None:
        self.rx_buffer.extend(received)
        while b"\n" in self.rx_buffer:
            raw_line, _, remainder = self.rx_buffer.partition(b"\n")
            self.rx_buffer[:] = remainder
            raw_line = raw_line.rstrip(b"\r")
            while raw_line and not (0x20 <= raw_line[0] <= 0x7E):
                raw_line = raw_line[1:]
            text = raw_line.decode("ascii", errors="replace")
            self.consume_line(text)

    def consume_line(self, text: str) -> None:
        self.log("RX", text)
        if text == "VISION_READY":
            self.ready = True
            return
        if text.startswith("TV,"):
            self.consume_tv(text)
            return
        if text.startswith("MS,"):
            self.consume_ms(text)
            return
        self.unknown_line_count += 1

    def consume_tv(self, text: str) -> None:
        fields = text.split(",")
        self.tv_line_count += 1
        if self.current_result is None or len(fields) < 9 or fields[6] != "CS":
            return
        try:
            pitch_submit = int(fields[7])
            yaw_submit = int(fields[8])
        except ValueError:
            return
        self.current_result.tv_lines += 1
        self.current_result.pitch_submit_count += pitch_submit
        self.current_result.yaw_submit_count += yaw_submit

    def consume_ms(self, text: str) -> None:
        fields = text.split(",")
        if len(fields) != 10 or fields[1] not in ("P", "Y"):
            self.unknown_line_count += 1
            return
        try:
            values = {name: int(value) for name, value in zip(COUNTER_NAMES, fields[2:])}
        except ValueError:
            self.unknown_line_count += 1
            return
        self.ms_line_count += 1
        axis = fields[1]
        self.motor_counters[axis] = values
        previous_protocol_errors = self.protocol_error_counts.get(axis, 0)
        self.protocol_error_counts[axis] = values["E"]
        if values["E"] > previous_protocol_errors:
            self.protocol_error_observed = True
            self.log(
                "WARN",
                f"motor {axis} protocol errors increased: "
                f"{previous_protocol_errors}->{values['E']}; continuing test",
            )
        if values["P"] != 0:
            self.fault = (
                f"motor {axis} reported protection error: P={values['P']}"
            )


def pump_rx(uart: serial.Serial, capture: Capture) -> None:
    waiting = uart.in_waiting
    if waiting:
        capture.consume_bytes(uart.read(waiting))


def wait_for_ready(
    uart: serial.Serial,
    capture: Capture,
    timeout_s: float,
) -> bool:
    deadline_s = time.perf_counter() + timeout_s
    capture.log("INFO", f"waiting for VISION_READY, timeout={timeout_s:.1f}s")
    while time.perf_counter() < deadline_s:
        pump_rx(uart, capture)
        if capture.ready:
            return True
        time.sleep(0.001)
    return False


def run_scenario(
    uart: serial.Serial,
    capture: Capture,
    scenario: Scenario,
    cycle: int,
    fps: float,
) -> ScenarioResult:
    result = ScenarioResult(
        cycle=cycle,
        name=scenario.name,
        dx=scenario.dx,
        dy=scenario.dy,
        duration_s=scenario.duration_s,
        counter_start=copy_counters(capture.motor_counters),
    )
    capture.current_result = result
    capture.log(
        "SIM",
        f"cycle={cycle} {scenario.name} dx={scenario.dx} dy={scenario.dy}",
    )

    frame = aim_frame(scenario.dx, scenario.dy)
    period_s = 1.0 / fps
    deadline_s = time.perf_counter() + scenario.duration_s
    next_tx_s = time.perf_counter()
    while time.perf_counter() < deadline_s:
        now_s = time.perf_counter()
        if now_s >= next_tx_s:
            uart.write(frame)
            next_tx_s += period_s
            if next_tx_s < now_s:
                next_tx_s = now_s + period_s
        pump_rx(uart, capture)
        if capture.fault is not None:
            break
        remaining_s = next_tx_s - time.perf_counter()
        if remaining_s > 0.001:
            time.sleep(min(remaining_s / 2.0, 0.002))

    pump_rx(uart, capture)
    result.counter_end = copy_counters(capture.motor_counters)
    result.counter_delta = counter_delta(result.counter_start, result.counter_end)
    capture.current_result = None
    return result


def run_trajectory(
    uart: serial.Serial,
    capture: Capture,
    samples: list[TrajectorySample],
    cycle: int,
    fps: float,
) -> TrajectoryResult:
    result = TrajectoryResult(
        cycle=cycle,
        name="CONTINUOUS_ELLIPSE",
        duration_s=len(samples) / fps,
        sample_count=len(samples),
        counter_start=copy_counters(capture.motor_counters),
    )
    capture.current_result = result
    capture.log(
        "SIM",
        f"cycle={cycle} CONTINUOUS_ELLIPSE samples={len(samples)} fps={fps:g}",
    )

    started_s = time.perf_counter()
    for sample in samples:
        target_send_s = started_s + sample.index / fps
        while time.perf_counter() < target_send_s:
            pump_rx(uart, capture)
            if capture.fault is not None:
                break
            remaining_s = target_send_s - time.perf_counter()
            if remaining_s > 0.001:
                time.sleep(min(remaining_s / 2.0, 0.002))
        if capture.fault is not None:
            break
        uart.write(aim_frame(sample.dx, sample.dy))
        pump_rx(uart, capture)

    pump_rx(uart, capture)
    result.counter_end = copy_counters(capture.motor_counters)
    result.counter_delta = counter_delta(result.counter_start, result.counter_end)
    capture.current_result = None
    return result


def write_summary(path: Path, summary: dict) -> None:
    path.write_text(
        json.dumps(summary, ensure_ascii=False, indent=2) + "\n",
        encoding="utf-8",
    )


def main() -> int:
    args = parse_args()
    if args.list_ports:
        for port in list_ports.comports():
            print(f"{port.device}: {port.description} [{port.hwid}]")
        return 0
    if (
        args.fps <= 0.0
        or args.cycles <= 0
        or args.ready_timeout <= 0.0
        or args.pre_send_delay < 0.0
        or args.trajectory_duration <= 0.0
        or args.trajectory_period <= 0.0
        or args.trajectory_ramp < 0.0
    ):
        raise SystemExit(
            "--fps, --cycles and --ready-timeout must be positive; "
            "trajectory duration/period must be positive; delays must be non-negative"
        )
    if not (0 <= args.yaw_amplitude < FRAME_WIDTH // 2):
        raise SystemExit("--yaw-amplitude must be between 0 and 255 pixels")
    if not (0 <= args.pitch_amplitude < FRAME_HEIGHT // 2):
        raise SystemExit("--pitch-amplitude must be between 0 and 159 pixels")

    trajectory_samples: list[TrajectorySample] = []
    if args.mode == "trajectory" or args.generate_only is not None:
        trajectory_samples = generate_continuous_error_array(
            args.fps,
            args.trajectory_duration,
            args.trajectory_period,
            args.yaw_amplitude,
            args.pitch_amplitude,
            args.trajectory_ramp,
        )
    if args.generate_only is not None:
        write_trajectory_csv(args.generate_only, trajectory_samples)
        print(
            f"generated {len(trajectory_samples)} samples at {args.fps:g} Hz: "
            f"{args.generate_only.resolve()}"
        )
        return 0

    if args.mode == "trajectory":
        longest_frame = max(
            len(aim_frame(item.dx, item.dy)) for item in trajectory_samples
        )
    else:
        longest_frame = max(len(aim_frame(item.dx, item.dy)) for item in SCENARIOS)
    wire_time_s = longest_frame * 10.0 / args.baud
    period_s = 1.0 / args.fps
    if period_s < wire_time_s:
        print(
            f"warning: {args.fps:.1f} Hz period is shorter than UART wire time "
            f"({wire_time_s * 1000.0:.3f} ms)",
            file=sys.stderr,
        )

    timestamp = datetime.now().strftime("%Y%m%d-%H%M%S")
    session_dir = args.log_dir / f"{timestamp}_{args.fps:g}Hz_{args.mode}"
    session_dir.mkdir(parents=True, exist_ok=False)
    raw_path = session_dir / "serial.log"
    summary_path = session_dir / "summary.json"
    results: list[ScenarioResult | TrajectoryResult] = []
    status = "not_started"
    error = ""

    with raw_path.open("w", encoding="utf-8", newline="\n") as raw_log:
        capture = Capture(raw_log)
        try:
            if args.mode == "trajectory":
                write_trajectory_csv(session_dir / "trajectory.csv", trajectory_samples)
            with serial.Serial(
                port=args.port,
                baudrate=args.baud,
                bytesize=serial.EIGHTBITS,
                parity=serial.PARITY_NONE,
                stopbits=serial.STOPBITS_ONE,
                timeout=0,
                write_timeout=1.0,
                rtscts=False,
                dsrdtr=False,
            ) as uart:
                uart.reset_input_buffer()
                uart.reset_output_buffer()
                capture.log(
                    "INFO",
                    f"opened {args.port} {args.baud} 8N1, requested fps={args.fps:g}",
                )
                if not wait_for_ready(uart, capture, args.ready_timeout):
                    status = "vision_ready_timeout"
                    error = "VISION_READY was not received"
                else:
                    status = "running"
                    if args.pre_send_delay > 0.0:
                        capture.log(
                            "INFO",
                            f"passive pre-send window={args.pre_send_delay:.1f}s",
                        )
                        passive_deadline_s = (
                            time.perf_counter() + args.pre_send_delay
                        )
                        while time.perf_counter() < passive_deadline_s:
                            pump_rx(uart, capture)
                            if capture.fault is not None:
                                break
                            time.sleep(0.001)
                    for cycle in range(1, args.cycles + 1):
                        if args.mode == "trajectory":
                            results.append(
                                run_trajectory(
                                    uart,
                                    capture,
                                    trajectory_samples,
                                    cycle,
                                    args.fps,
                                )
                            )
                        else:
                            for scenario in SCENARIOS:
                                results.append(
                                    run_scenario(
                                        uart, capture, scenario, cycle, args.fps
                                    )
                                )
                                if capture.fault is not None:
                                    break
                        if capture.fault is not None:
                            break

                    final_center = Scenario(
                        "FINAL_CENTER", 0, 0, FINAL_CENTER_DURATION_S
                    )
                    results.append(
                        run_scenario(uart, capture, final_center, args.cycles, args.fps)
                    )
                    if capture.fault is not None:
                        status = "motor_fault"
                        error = capture.fault
                    elif capture.protocol_error_observed:
                        status = "complete_with_protocol_errors"
                        error = (
                            "motor response protocol errors were recorded; "
                            "trajectory transmission was not aborted"
                        )
                    else:
                        status = "complete"
        except (OSError, serial.SerialException) as exc:
            status = "serial_error"
            error = str(exc)
            capture.log("ERROR", error)

        summary = {
            "status": status,
            "error": error,
            "port": args.port,
            "baud": args.baud,
            "fps": args.fps,
            "cycles": args.cycles,
            "mode": args.mode,
            "pre_send_delay_s": args.pre_send_delay,
            "frame_bytes": longest_frame,
            "wire_time_ms": wire_time_s * 1000.0,
            "trajectory": {
                "duration_s": args.trajectory_duration,
                "period_s": args.trajectory_period,
                "yaw_amplitude_pixels": args.yaw_amplitude,
                "pitch_amplitude_pixels": args.pitch_amplitude,
                "ramp_s": args.trajectory_ramp,
                "sample_count": len(trajectory_samples),
            }
            if args.mode == "trajectory"
            else None,
            "vision_ready": capture.ready,
            "tv_line_count": capture.tv_line_count,
            "ms_line_count": capture.ms_line_count,
            "unknown_line_count": capture.unknown_line_count,
            "protocol_error_observed": capture.protocol_error_observed,
            "final_motor_counters": capture.motor_counters,
            "scenarios": [asdict(item) for item in results],
        }
        write_summary(summary_path, summary)
        capture.log("INFO", f"status={status} summary={summary_path}")

    print(f"SESSION_DIR={session_dir}")
    return 0 if status in ("complete", "complete_with_protocol_errors") else 2


if __name__ == "__main__":
    raise SystemExit(main())
