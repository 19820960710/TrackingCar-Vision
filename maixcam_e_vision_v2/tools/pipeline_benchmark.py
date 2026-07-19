"""Run integrated V2 timing measurements on MaixCAM Pro."""

from app.timing import now_us
from main import build_runtime
from settings import PIPELINE_BENCHMARK_SAMPLE_FRAMES, PIPELINE_TARGET_P95_BUDGET_US


def main():
    runtime, plane_mapper = build_runtime()
    started = now_us()
    observation = None
    for _ in range(PIPELINE_BENCHMARK_SAMPLE_FRAMES):
        observation = runtime.run_once({"geometry_enabled": plane_mapper is not None, "circle_requested": False})
    performance = observation["performance"]
    print(
        "PIPELINE,frames=%d,target_p95_us=%d,ai_recapture_p95_us=%d,laser_p95_us=%d,"
        "geometry_p95_us=%d,uart_write_p95_us=%d,capture_p95_us=%d,frame_cpu_p95_us=%d,target_budget_us=%d"
        % (
            PIPELINE_BENCHMARK_SAMPLE_FRAMES,
            performance.get("target", {}).get("p95_us", 0),
            performance.get("ai_recapture", {}).get("p95_us", 0),
            performance.get("laser", {}).get("p95_us", 0),
            performance.get("geometry", {}).get("p95_us", 0),
            performance.get("uart_write", {}).get("p95_us", 0),
            performance.get("capture", {}).get("p95_us", 0),
            performance.get("frame_cpu", {}).get("p95_us", 0),
            PIPELINE_TARGET_P95_BUDGET_US,
        )
    )
    print("PIPELINE elapsed_us=%d; fill end-to-end and MSPM0 UART receive timestamps in docs/pipeline_benchmark.md" % (now_us() - started))


if __name__ == "__main__":
    main()
