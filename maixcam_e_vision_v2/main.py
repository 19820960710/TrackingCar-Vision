"""MaixCAM Pro V2 integrated visual runtime."""

from app.pipeline import VisionPipeline
from app.runtime import VisionRuntime
from app.timing import PerformanceStats
from drivers.camera_device import apply_initial_tuning, create_camera, warmup
from drivers.display import create_display
from drivers.uart_device import create_uart
from protocol.ascii_aim import encode as ascii_encode
from settings import APP_VERSION, MSPM0_STALE_MS, PIPELINE_TIMING_WINDOW, UART_PROTOCOL_MODE
from vision.geometry.standard_plane import CalibrationRequired, MeasuredTargetDimensions, StandardPlaneMapper
from vision.laser.detector import LaserDetector
from vision.target.detector import TargetDetector


def encode_ascii_for_mspm0(target, laser):
    """Bind the generic two-measurement pipeline interface to the configured UART freshness limit."""
    return ascii_encode(target, laser, max_age_ms=MSPM0_STALE_MS)


def optional_plane_mapper():
    try:
        return StandardPlaneMapper(MeasuredTargetDimensions.from_settings())
    except CalibrationRequired:
        print("mm geometry disabled: fill measured PLANE_* dimensions first")
        return None


def build_runtime():
    if UART_PROTOCOL_MODE != "ascii_aim":
        raise RuntimeError("binary_v1 is defined but not enabled until MSPM0 firmware acceptance")
    camera_device = create_camera()
    apply_initial_tuning(camera_device)
    warmup(camera_device)
    try:
        print("camera active: %dx%d @ %.1f fps, buffers=%d" % (
            camera_device.width(), camera_device.height(),
            camera_device.fps(), camera_device.buff_num(),
        ))
    except Exception as error:
        print("camera active settings unavailable: %s" % error)
    display_device = create_display()
    uart_device = create_uart()
    plane_mapper = optional_plane_mapper()
    pipeline = VisionPipeline(
        target_detector=TargetDetector(),
        laser_detector=LaserDetector(),
        protocol=encode_ascii_for_mspm0,
        uart_device=uart_device,
        plane_mapper=plane_mapper,
        performance=PerformanceStats(PIPELINE_TIMING_WINDOW),
    )
    return VisionRuntime(camera_device, pipeline, display_device), plane_mapper


def main():
    from maix import app
    runtime, plane_mapper = build_runtime()
    print("MaixCAM E Vision V2 integrated runtime: %s" % APP_VERSION)
    while not app.need_exit():
        observation = runtime.run_once({"geometry_enabled": plane_mapper is not None, "circle_requested": False})
        if observation["frame_index"] % 60 == 0:
            print("P95 capture=%dus target=%dus laser=%dus display=%dus cpu=%dus total=%dus" % (
                observation["performance"].get("capture", {}).get("p95_us", 0),
                observation["performance"].get("target", {}).get("p95_us", 0),
                observation["performance"].get("laser", {}).get("p95_us", 0),
                observation["performance"].get("display", {}).get("p95_us", 0),
                observation["performance"].get("frame_cpu", {}).get("p95_us", 0),
                observation["performance"].get("frame_total", {}).get("p95_us", 0),
            ))


if __name__ == "__main__":
    main()
