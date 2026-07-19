"""Run one camera candidate and periodic safe UART LOST frames on MaixCAM."""

import time

from drivers.camera_device import apply_initial_tuning, create_camera, warmup
from drivers.uart_device import create_uart
from settings import CAMERA_BENCHMARK_CANDIDATES, CAMERA_BENCHMARK_INDEX, CAMERA_BENCHMARK_SAMPLE_FRAMES, UART_TEST_FRAME, UART_TEST_SEND_EVERY_N_FRAMES
from tools.benchmark import percentile


def now_us():
    if hasattr(time, "ticks_us"):
        return time.ticks_us()
    return int(time.time() * 1000000)


def elapsed_us(newer, older):
    if hasattr(time, "ticks_diff"):
        return time.ticks_diff(newer, older)
    return newer - older


def selected_candidate():
    if CAMERA_BENCHMARK_INDEX < 0 or CAMERA_BENCHMARK_INDEX >= len(CAMERA_BENCHMARK_CANDIDATES):
        raise ValueError("CAMERA_BENCHMARK_INDEX must select one candidate")
    return CAMERA_BENCHMARK_CANDIDATES[CAMERA_BENCHMARK_INDEX]


def main():
    width, height, fps = selected_candidate()
    camera_device = create_camera(width, height, fps)
    apply_initial_tuning(camera_device)
    warmup(camera_device)
    uart_device = create_uart()
    intervals = []
    read_costs = []
    uart_write_costs = []
    previous = now_us()
    started = previous
    for frame_index in range(CAMERA_BENCHMARK_SAMPLE_FRAMES):
        read_started = now_us()
        camera_device.read()
        read_costs.append(elapsed_us(now_us(), read_started))
        current = now_us()
        if frame_index:
            intervals.append(elapsed_us(current, previous))
        previous = current
        if frame_index % UART_TEST_SEND_EVERY_N_FRAMES == 0:
            write_started = now_us()
            uart_device.write_str(UART_TEST_FRAME)
            uart_write_costs.append(elapsed_us(now_us(), write_started))
    elapsed = elapsed_us(now_us(), started)
    fps_actual = (CAMERA_BENCHMARK_SAMPLE_FRAMES * 1000000.0 / elapsed) if elapsed > 0 else 0.0
    print(
        "BASELINE,width=%d,height=%d,requested_fps=%d,frames=%d,actual_fps=%.2f,"
        "cycle_p50_us=%d,cycle_p95_us=%d,read_p50_us=%d,read_p95_us=%d,"
        "uart_write_p50_us=%d,uart_write_p95_us=%d,uart_every_frames=%d"
        % (
            width, height, fps, CAMERA_BENCHMARK_SAMPLE_FRAMES, fps_actual,
            percentile(intervals, 0.50), percentile(intervals, 0.95),
            percentile(read_costs, 0.50), percentile(read_costs, 0.95),
            percentile(uart_write_costs, 0.50), percentile(uart_write_costs, 0.95),
            UART_TEST_SEND_EVERY_N_FRAMES,
        )
    )
    print("MSPM0 must confirm only valid AIM frames were parsed and no restart occurred.")


if __name__ == "__main__":
    main()
