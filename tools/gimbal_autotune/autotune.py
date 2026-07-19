"""One-flash CMSIS-DAP autotuner for the camera gimbal PD loop."""
from __future__ import annotations

import argparse
import csv
import ctypes
import json
import math
import re
import statistics
import struct
import subprocess
import sys
import time
from dataclasses import asdict, dataclass
from datetime import datetime
from pathlib import Path
from typing import Iterable

MAX_TRIALS = 8
MAX_TRACE = 64
IDLE, RUNNING, COMPLETE, FAILED = 0, 1, 2, 3
MAILBOX_SYMBOL = "g_gimbal_autotune"
# Flashing is handled by Keil's MSPM0 algorithm. pyOCD only needs generic
# Cortex-M memory access for the live RAM mailbox.
TARGET = "cortex_m"
DEFAULT_SEED = 20260719


class Command(ctypes.LittleEndianStructure):
    _fields_ = [
        ("request_seq", ctypes.c_uint32),
        ("request_seq_inv", ctypes.c_uint32),
        ("abort_seq", ctypes.c_uint32),
        ("yaw_kp", ctypes.c_float),
        ("yaw_kd", ctypes.c_float),
        ("pitch_kp", ctypes.c_float),
        ("pitch_kd", ctypes.c_float),
        ("seed", ctypes.c_uint32),
        ("trial_count", ctypes.c_uint32),
        ("yaw_max_pulses", ctypes.c_uint32),
        ("pitch_max_pulses", ctypes.c_uint32),
        ("offset_timeout_ms", ctypes.c_uint32),
        ("track_timeout_ms", ctypes.c_uint32),
        ("target_lost_ms", ctypes.c_uint32),
        ("stable_samples", ctypes.c_uint32),
    ]


class Trial(ctypes.LittleEndianStructure):
    _fields_ = [
        ("yaw_offset", ctypes.c_int32),
        ("pitch_offset", ctypes.c_int32),
        ("offset_ms", ctypes.c_uint32),
        ("return_ms", ctypes.c_uint32),
        ("yaw_settle_ms", ctypes.c_uint32),
        ("pitch_settle_ms", ctypes.c_uint32),
        ("valid_samples", ctypes.c_uint32),
        ("target_lost_samples", ctypes.c_uint32),
        ("yaw_max_error", ctypes.c_float),
        ("yaw_stable_mean_error", ctypes.c_float),
        ("yaw_zero_crossings", ctypes.c_uint32),
        ("yaw_max_pulse_step", ctypes.c_uint32),
        ("pitch_max_error", ctypes.c_float),
        ("pitch_stable_mean_error", ctypes.c_float),
        ("pitch_zero_crossings", ctypes.c_uint32),
        ("pitch_max_pulse_step", ctypes.c_uint32),
        ("yaw_position_before", ctypes.c_int32),
        ("yaw_position_after", ctypes.c_int32),
        ("pitch_position_before", ctypes.c_int32),
        ("pitch_position_after", ctypes.c_int32),
        ("yaw_response_function", ctypes.c_uint32),
        ("yaw_response_code", ctypes.c_uint32),
        ("pitch_response_function", ctypes.c_uint32),
        ("pitch_response_code", ctypes.c_uint32),
        ("status", ctypes.c_uint32),
    ]


class Trace(ctypes.LittleEndianStructure):
    _fields_ = [
        ("trial", ctypes.c_uint32),
        ("elapsed_ms", ctypes.c_uint32),
        ("target_valid", ctypes.c_uint32),
        ("dx", ctypes.c_int32),
        ("dy", ctypes.c_int32),
        ("yaw_pulses", ctypes.c_int32),
        ("pitch_pulses", ctypes.c_int32),
    ]


class Mailbox(ctypes.LittleEndianStructure):
    _fields_ = [
        ("snapshot_seq", ctypes.c_uint32),
        ("ack_seq", ctypes.c_uint32),
        ("status", ctypes.c_uint32),
        ("phase", ctypes.c_uint32),
        ("current_trial", ctypes.c_uint32),
        ("failed_trials", ctypes.c_uint32),
        ("trace_count", ctypes.c_uint32),
        ("trace_overflow", ctypes.c_uint32),
        ("applied_yaw_kp", ctypes.c_float),
        ("applied_yaw_kd", ctypes.c_float),
        ("applied_pitch_kp", ctypes.c_float),
        ("applied_pitch_kd", ctypes.c_float),
        ("observed_target_valid", ctypes.c_uint32),
        ("observed_dx", ctypes.c_int32),
        ("observed_dy", ctypes.c_int32),
        ("result", Trial * MAX_TRIALS),
        ("trace", Trace * MAX_TRACE),
        ("command", Command),
    ]


@dataclass(frozen=True)
class Candidate:
    yaw_kp: float
    yaw_kd: float
    pitch_kp: float
    pitch_kd: float

    def label(self) -> str:
        return (f"ykp{self.yaw_kp:.4f}_ykd{self.yaw_kd:.4f}_"
                f"pkp{self.pitch_kp:.4f}_pkd{self.pitch_kd:.4f}")


@dataclass(frozen=True)
class AxisScore:
    valid: bool
    score: float
    successes: int
    median_settle_ms: float
    p95_settle_ms: float
    stable_mean_error: float
    mean_zero_crossings: float
    max_pulse_step: int


def map_symbol(path: Path, name: str) -> tuple[int, int]:
    match = re.search(
        rf"^\s*{re.escape(name)}\s+(0x[0-9a-fA-F]+)\s+Data\s+(\d+)\b",
        path.read_text(errors="replace"), re.MULTILINE)
    if not match:
        raise RuntimeError(f"{name} not found in {path}")
    return int(match.group(1), 16), int(match.group(2))


def percentile(values: list[float], percent: float) -> float:
    ordered = sorted(values)
    if not ordered:
        return math.inf
    index = (len(ordered) - 1) * percent
    lower = math.floor(index)
    upper = math.ceil(index)
    if lower == upper:
        return ordered[lower]
    return ordered[lower] + (ordered[upper] - ordered[lower]) * (index - lower)


def axis_score(trials: Iterable[Trial], axis: str, trial_count: int) -> AxisScore:
    selected = list(trials)[:trial_count]
    good = [trial for trial in selected if trial.status == 0]
    required = math.ceil(0.8 * trial_count)
    if len(good) < required:
        return AxisScore(False, math.inf, len(good), math.inf, math.inf,
                         math.inf, math.inf, 0)
    settle = [float(getattr(trial, f"{axis}_settle_ms")) for trial in good]
    stable = [float(getattr(trial, f"{axis}_stable_mean_error")) for trial in good]
    crossings = [float(getattr(trial, f"{axis}_zero_crossings")) for trial in good]
    steps = [int(getattr(trial, f"{axis}_max_pulse_step")) for trial in good]
    median_ms = statistics.median(settle)
    p95_ms = percentile(settle, 0.95)
    stable_mean = statistics.fmean(stable)
    crossing_mean = statistics.fmean(crossings)
    max_step = max(steps, default=0)
    score = (median_ms + 0.25 * p95_ms + 40.0 * stable_mean +
             20.0 * crossing_mean + 0.10 * max_step)
    return AxisScore(True, score, len(good), median_ms, p95_ms,
                     stable_mean, crossing_mean, max_step)


def write_csv(path: Path, rows: list[dict]) -> None:
    if not rows:
        return
    with path.open("w", newline="", encoding="utf-8-sig") as handle:
        writer = csv.DictWriter(handle, fieldnames=list(rows[0]))
        writer.writeheader()
        writer.writerows(rows)


def trace_rows(mailbox: Mailbox) -> list[dict]:
    return [
        {
            "trial": item.trial,
            "elapsed_ms": item.elapsed_ms,
            "target_valid": item.target_valid,
            "dx": item.dx,
            "dy": item.dy,
            "yaw_pulses": item.yaw_pulses,
            "pitch_pulses": item.pitch_pulses,
        }
        for item in list(mailbox.trace)[:mailbox.trace_count]
    ]


def plot_trace(path: Path, candidate: Candidate, rows: list[dict]) -> None:
    if not rows:
        return
    import matplotlib.pyplot as plt
    figure, axes = plt.subplots(2, 1, figsize=(9, 6), sharex=True)
    elapsed = [row["elapsed_ms"] for row in rows]
    axes[0].plot(elapsed, [row["dx"] for row in rows], label="yaw dx")
    axes[0].plot(elapsed, [row["dy"] for row in rows], label="pitch dy")
    axes[0].axhline(0, color="black", linewidth=0.8)
    axes[0].legend()
    axes[1].plot(elapsed, [row["yaw_pulses"] for row in rows], label="yaw")
    axes[1].plot(elapsed, [row["pitch_pulses"] for row in rows], label="pitch")
    axes[1].legend()
    axes[1].set_xlabel("elapsed ms")
    figure.suptitle(candidate.label())
    figure.tight_layout()
    figure.savefig(path, dpi=150)
    plt.close(figure)


class Evaluator:
    def __init__(self, map_path: Path, probe_uid: str | None = None):
        print("connecting CMSIS-DAP probe...", flush=True)
        from pyocd.core.helpers import ConnectHelper
        from pyocd.core.target import Target
        self.Target = Target
        self.address, size = map_symbol(map_path, MAILBOX_SYMBOL)
        expected = ctypes.sizeof(Mailbox)
        if size != expected:
            raise RuntimeError(f"mailbox size map={size}, python={expected}")
        self.session = ConnectHelper.session_with_chosen_probe(
            unique_id=probe_uid,
            target_override=TARGET,
            options={"connect_mode": "attach", "frequency": 1_000_000,
                     "resume_on_disconnect": True},
            return_first=True)
        if self.session is None:
            raise RuntimeError("CMSIS-DAP probe was not found")
        print("opening debug session...", flush=True)
        self.session.open()
        print("resetting tuning firmware...", flush=True)
        self.target = self.session.target
        # A Keil download leaves this target halted at the old PC. Resuming
        # without reset continues from stale execution state in newly written
        # flash, so always start the tuning firmware from its reset vector.
        self.target.reset_and_halt()
        self.target.resume()
        time.sleep(0.5)
        print("tuning firmware running", flush=True)

    def close(self) -> None:
        # Do not rely on probe/session defaults: a failed run must never leave
        # the FreeRTOS target halted and make the user press RESET to recover.
        if self.target.get_state() == self.Target.State.HALTED:
            self.target.resume()
            time.sleep(0.05)
        self.session.close()

    def read(self) -> Mailbox:
        was_running = self.target.get_state() != self.Target.State.HALTED
        if was_running:
            self.target.halt()
        try:
            words = self.target.read_memory_block32(
                self.address, ctypes.sizeof(Mailbox) // 4)
            return Mailbox.from_buffer_copy(
                struct.pack(f"<{len(words)}I", *words))
        finally:
            if was_running:
                self.target.resume()

    def read_header(self) -> dict[str, int]:
        for _ in range(20):
            before = self.target.read32(self.address)
            if before & 1:
                time.sleep(0.005)
                continue
            words = self.target.read_memory_block32(self.address, 15)
            after = self.target.read32(self.address)
            if before == after and not (after & 1):
                return {
                    "ack_seq": words[1],
                    "status": words[2],
                    "observed_target_valid": words[12],
                    "observed_dx": ctypes.c_int32(words[13]).value,
                    "observed_dy": ctypes.c_int32(words[14]).value,
                }
            time.sleep(0.005)
        raise RuntimeError("no coherent mailbox header")

    def preflight(self) -> None:
        valid = 0
        for _ in range(10):
            header = self.read_header()
            valid += int(bool(header["observed_target_valid"]))
            time.sleep(0.05)
        if valid < 8:
            raise RuntimeError(
                "target preflight failed: fewer than 8/10 valid observations")

    def write_command(self, command: Command) -> None:
        offset = Mailbox.command.offset
        body = struct.unpack(
            f"<{(ctypes.sizeof(Command) - 8) // 4}I", bytes(command)[8:])
        self.target.halt()
        self.target.write_memory_block32(self.address + offset + 8, list(body))
        self.target.write32(self.address + offset + 4,
                            command.request_seq_inv)
        self.target.write32(self.address + offset, command.request_seq)
        self.target.resume()

    def abort(self, request_seq: int) -> None:
        if request_seq == 0:
            return
        self.target.halt()
        self.target.write32(self.address + Mailbox.command.offset + 8,
                            request_seq)
        self.target.resume()

    def evaluate(self, candidate: Candidate, trials: int, seed: int,
                 axis: str = "both", pulse_limit: int = 120) -> Mailbox:
        if not 120 <= pulse_limit <= 400:
            raise ValueError("pulse_limit must be between 120 and 400")
        current = self.read_header()
        command = Command()
        command.request_seq = ((current["ack_seq"] + 1) & 0xFFFFFFFF) or 1
        command.request_seq_inv = (~command.request_seq) & 0xFFFFFFFF
        command.abort_seq = 0
        command.yaw_kp = candidate.yaw_kp
        command.yaw_kd = candidate.yaw_kd
        command.pitch_kp = candidate.pitch_kp
        command.pitch_kd = candidate.pitch_kd
        command.seed = seed
        command.trial_count = trials
        command.yaw_max_pulses = (
            pulse_limit if axis in ("yaw", "both") else 0)
        command.pitch_max_pulses = (
            pulse_limit if axis in ("pitch", "both") else 0)
        command.offset_timeout_ms = 4000
        command.track_timeout_ms = 5000
        command.target_lost_ms = 1000
        command.stable_samples = 8
        print(f"  {candidate.label()} trials={trials} seed={seed}")
        self.write_command(command)
        deadline = time.monotonic() + trials * 10 + 10
        try:
            while time.monotonic() < deadline:
                header = self.read_header()
                if (header["ack_seq"] == command.request_seq and
                        header["status"] in (COMPLETE, FAILED)):
                    return self.read()
                time.sleep(0.05)
        except BaseException:
            self.abort(command.request_seq)
            raise
        self.abort(command.request_seq)
        raise RuntimeError("gimbal mailbox trial timed out")


class RunRecorder:
    def __init__(self, output: Path):
        self.output = output
        self.output.mkdir(parents=True)
        self.evaluations: list[dict] = []
        self.trials: list[dict] = []

    def record(self, stage: str, candidate: Candidate, seed: int,
               mailbox: Mailbox) -> tuple[AxisScore, AxisScore]:
        trial_count = mailbox.command.trial_count
        yaw = axis_score(mailbox.result, "yaw", trial_count)
        pitch = axis_score(mailbox.result, "pitch", trial_count)
        row = {
            "stage": stage, "candidate": candidate.label(), "seed": seed,
            **asdict(candidate), "trial_count": trial_count,
            "failed_trials": mailbox.failed_trials,
            "trace_overflow": mailbox.trace_overflow,
            "yaw_score": yaw.score, "pitch_score": pitch.score,
            "yaw_successes": yaw.successes,
            "pitch_successes": pitch.successes,
            "yaw_median_ms": yaw.median_settle_ms,
            "pitch_median_ms": pitch.median_settle_ms,
            "yaw_p95_ms": yaw.p95_settle_ms,
            "pitch_p95_ms": pitch.p95_settle_ms,
            "yaw_stable_error": yaw.stable_mean_error,
            "pitch_stable_error": pitch.stable_mean_error,
        }
        self.evaluations.append(row)
        for index, trial in enumerate(list(mailbox.result)[:trial_count]):
            self.trials.append({
                "stage": stage, "candidate": candidate.label(), "seed": seed,
                "trial": index,
                **{name: getattr(trial, name)
                   for name, _ in Trial._fields_},
            })
        trace = trace_rows(mailbox)
        stem = f"{stage}_{candidate.label()}_seed{seed}"
        write_csv(self.output / f"{stem}_trace.csv", trace)
        plot_trace(self.output / f"{stem}.png", candidate, trace)
        self.flush()
        return yaw, pitch

    def flush(self) -> None:
        write_csv(self.output / "evaluations.csv", self.evaluations)
        write_csv(self.output / "trials.csv", self.trials)


def run_uv4(uv4: Path, project: Path) -> None:
    for switch in ("-r", "-f"):
        result = subprocess.run([str(uv4), switch, str(project), "-j0"],
                                check=False)
        if result.returncode:
            raise RuntimeError(f"Keil {switch} failed: {result.returncode}")


def grid(center: Candidate, axis: str, kp_values: Iterable[float],
         kd_values: Iterable[float]) -> list[Candidate]:
    candidates = []
    for kp in kp_values:
        for kd in kd_values:
            if axis == "yaw":
                candidates.append(Candidate(kp, kd, center.pitch_kp,
                                            center.pitch_kd))
            else:
                candidates.append(Candidate(center.yaw_kp, center.yaw_kd,
                                            kp, kd))
    return candidates


def choose_axis(evaluator: Evaluator, recorder: RunRecorder, stage: str,
                candidates: list[Candidate], axis: str, trials: int,
                seed: int, pulse_limit: int = 400) -> Candidate:
    ranked: list[tuple[float, Candidate]] = []
    for candidate in candidates:
        mailbox = evaluator.evaluate(candidate, trials, seed, axis,
                                     pulse_limit)
        yaw, pitch = recorder.record(stage, candidate, seed, mailbox)
        if mailbox.result[0].status == 5:
            raise RuntimeError(
                f"{axis} perturbation did not leave the image deadband")
        score = yaw if axis == "yaw" else pitch
        if score.valid:
            ranked.append((score.score, candidate))
    if not ranked:
        raise RuntimeError(f"no valid {axis} candidate in {stage}")
    ranked.sort(key=lambda item: item[0])
    return ranked[0][1]


def validate_candidate(evaluator: Evaluator, recorder: RunRecorder,
                       candidate: Candidate) -> dict:
    mailboxes = []
    for index, seed in enumerate((DEFAULT_SEED, DEFAULT_SEED + 1)):
        mailbox = evaluator.evaluate(candidate, 8, seed, "both", 400)
        recorder.record(f"validation_{index}", candidate, seed, mailbox)
        mailboxes.append(mailbox)
    trials = [trial for mailbox in mailboxes for trial in list(mailbox.result)[:8]]
    successes = sum(trial.status == 0 for trial in trials)
    yaw = axis_score(trials, "yaw", 16)
    pitch = axis_score(trials, "pitch", 16)
    accepted = (
        successes >= 15 and yaw.valid and pitch.valid and
        yaw.median_settle_ms <= 1200 and pitch.median_settle_ms <= 1200 and
        yaw.p95_settle_ms <= 1800 and pitch.p95_settle_ms <= 1800 and
        yaw.stable_mean_error <= 3 and pitch.stable_mean_error <= 3 and
        yaw.mean_zero_crossings <= 2 and pitch.mean_zero_crossings <= 2 and
        yaw.max_pulse_step <= 800 and pitch.max_pulse_step <= 800)
    return {
        "accepted": accepted,
        "successes": successes,
        "trials": 16,
        "yaw": asdict(yaw),
        "pitch": asdict(pitch),
    }


def validate_candidate_quick(evaluator: Evaluator, recorder: RunRecorder,
                             candidate: Candidate) -> dict:
    """Short hardware check for producing an initial usable gain set."""
    yaw_box = evaluator.evaluate(candidate, 3, DEFAULT_SEED + 1, "yaw", 120)
    recorder.record("quick_validation_yaw", candidate, DEFAULT_SEED + 1,
                    yaw_box)
    pitch_box = evaluator.evaluate(candidate, 3, DEFAULT_SEED + 2, "pitch", 120)
    recorder.record("quick_validation_pitch", candidate, DEFAULT_SEED + 2,
                    pitch_box)
    yaw = axis_score(yaw_box.result, "yaw", 3)
    pitch = axis_score(pitch_box.result, "pitch", 3)
    accepted = (
        yaw.valid and pitch.valid and
        yaw.p95_settle_ms <= 3000 and pitch.p95_settle_ms <= 3000 and
        yaw.stable_mean_error <= 5 and pitch.stable_mean_error <= 5 and
        yaw.mean_zero_crossings <= 4 and pitch.mean_zero_crossings <= 4)
    return {
        "accepted": accepted,
        "quick": True,
        "trials": 6,
        "yaw": asdict(yaw),
        "pitch": asdict(pitch),
    }


def run_self_test() -> None:
    assert ctypes.sizeof(Mailbox) % 4 == 0
    mailbox = Mailbox()
    mailbox.command.trial_count = 5
    for index in range(5):
        trial = mailbox.result[index]
        trial.status = 0
        trial.yaw_settle_ms = 800 + index * 10
        trial.pitch_settle_ms = 900 + index * 10
        trial.yaw_stable_mean_error = 1.5
        trial.pitch_stable_mean_error = 2.0
        trial.yaw_max_pulse_step = 100
        trial.pitch_max_pulse_step = 120
    assert axis_score(mailbox.result, "yaw", 5).valid
    packed = bytes(mailbox)
    assert Mailbox.from_buffer_copy(packed).command.trial_count == 5
    print(f"self-test passed; mailbox_size={ctypes.sizeof(Mailbox)}")


def parse_args() -> argparse.Namespace:
    root = Path(__file__).resolve().parents[2]
    parser = argparse.ArgumentParser()
    parser.add_argument("--self-test", action="store_true")
    parser.add_argument("--hardware", action="store_true")
    parser.add_argument("--yes-clear-gimbal", action="store_true")
    parser.add_argument("--skip-build-flash", action="store_true")
    parser.add_argument("--validate-only", action="store_true")
    parser.add_argument("--quick", action="store_true",
                        help="coarse grids only, three trials per candidate")
    parser.add_argument("--yaw-kp", type=float, default=0.20)
    parser.add_argument("--yaw-kd", type=float, default=0.008)
    parser.add_argument("--pitch-kp", type=float, default=0.40)
    parser.add_argument("--pitch-kd", type=float, default=0.005)
    parser.add_argument("--project", type=Path,
                        default=root / "keil" / "M0_Templant_FreeRTOS.uvprojx")
    parser.add_argument("--map", type=Path,
                        default=root / "keil" / "M0_Templant_FreeRTOS.map")
    parser.add_argument("--uv4", type=Path)
    parser.add_argument("--probe-uid")
    parser.add_argument("--output-root", type=Path,
                        default=root / "gimbal-autotune-results")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    if args.self_test:
        run_self_test()
        return 0
    if not args.hardware or not args.yes_clear_gimbal:
        raise SystemExit("hardware mode requires --hardware --yes-clear-gimbal")
    if not args.skip_build_flash:
        if args.uv4 is None or not args.uv4.is_file():
            raise SystemExit("provide --uv4 or use --skip-build-flash")
        run_uv4(args.uv4, args.project)

    output = args.output_root / datetime.now().strftime("%Y%m%d-%H%M%S")
    recorder = RunRecorder(output)
    metadata = {
        "started_at": datetime.now().isoformat(),
        "map": str(args.map.resolve()),
        "mailbox_size": ctypes.sizeof(Mailbox),
        "argv": sys.argv,
    }
    (output / "run_metadata.json").write_text(
        json.dumps(metadata, indent=2, ensure_ascii=False), encoding="utf-8")
    evaluator = Evaluator(args.map, args.probe_uid)
    selected = Candidate(args.yaw_kp, args.yaw_kd,
                         args.pitch_kp, args.pitch_kd)
    try:
        evaluator.preflight()
        if not args.validate_only:
            search_trials = 3 if args.quick else 5
            search_pulses = 120 if args.quick else 400
            selected = choose_axis(
                evaluator, recorder, "yaw_coarse",
                grid(selected, "yaw", (0.12, 0.20, 0.28),
                     (0.0, 0.006, 0.012)), "yaw", search_trials,
                DEFAULT_SEED, search_pulses)
            if not args.quick:
                selected = choose_axis(
                    evaluator, recorder, "yaw_fine",
                    grid(selected, "yaw",
                         (max(0.0, selected.yaw_kp - 0.04), selected.yaw_kp,
                          selected.yaw_kp + 0.04),
                         (max(0.0, selected.yaw_kd - 0.002), selected.yaw_kd,
                          selected.yaw_kd + 0.002)),
                    "yaw", search_trials, DEFAULT_SEED, search_pulses)
            selected = choose_axis(
                evaluator, recorder, "pitch_coarse",
                grid(selected, "pitch", (0.24, 0.40, 0.56),
                     (0.0, 0.005, 0.010)), "pitch", search_trials,
                DEFAULT_SEED, search_pulses)
            if not args.quick:
                selected = choose_axis(
                    evaluator, recorder, "pitch_fine",
                    grid(selected, "pitch",
                         (max(0.0, selected.pitch_kp - 0.08), selected.pitch_kp,
                          selected.pitch_kp + 0.08),
                         (max(0.0, selected.pitch_kd - 0.0025), selected.pitch_kd,
                          selected.pitch_kd + 0.0025)),
                    "pitch", search_trials, DEFAULT_SEED, search_pulses)
        validation = (validate_candidate_quick(evaluator, recorder, selected)
                      if args.quick else
                      validate_candidate(evaluator, recorder, selected))
        payload = {"candidate": asdict(selected), "validation": validation}
        (output / "selected_candidate.json").write_text(
            json.dumps(payload, indent=2, ensure_ascii=False), encoding="utf-8")
        print(json.dumps(payload, indent=2, ensure_ascii=False))
        return 0 if validation["accepted"] else 2
    except BaseException as error:
        (output / "failure.json").write_text(
            json.dumps({"type": type(error).__name__, "message": str(error)},
                       indent=2, ensure_ascii=False), encoding="utf-8")
        raise
    finally:
        recorder.flush()
        evaluator.close()


if __name__ == "__main__":
    raise SystemExit(main())
