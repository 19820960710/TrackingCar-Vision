"""Open a camera and show a live preview."""

import argparse
import sys
import time
from pathlib import Path

import cv2
import yaml

ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / "src"
if str(SRC) not in sys.path:
    sys.path.insert(0, str(SRC))

from camera import get_camera_info, open_camera, read_frame


def load_camera_config(config_path):
    if not config_path.exists():
        return {"id": 0, "width": 640, "height": 480, "fps": 30}

    with config_path.open("r", encoding="utf-8") as file:
        data = yaml.safe_load(file) or {}

    return data.get("camera", {})


def parse_args():
    parser = argparse.ArgumentParser(description="Check camera preview.")
    parser.add_argument("--config", default="configs/camera.example.yaml")
    parser.add_argument("--id", type=int, default=None, help="Camera id, usually 0 or 1.")
    parser.add_argument("--width", type=int, default=None)
    parser.add_argument("--height", type=int, default=None)
    parser.add_argument("--fps", type=int, default=None)
    return parser.parse_args()


def main():
    args = parse_args()
    config_path = ROOT / args.config
    camera_cfg = load_camera_config(config_path)

    camera_id = args.id if args.id is not None else int(camera_cfg.get("id", 0))
    width = args.width if args.width is not None else int(camera_cfg.get("width", 640))
    height = args.height if args.height is not None else int(camera_cfg.get("height", 480))
    fps = args.fps if args.fps is not None else int(camera_cfg.get("fps", 30))

    capture = open_camera(camera_id=camera_id, width=width, height=height, fps=fps)
    info = get_camera_info(capture)
    print(
        f"Camera {camera_id} opened: "
        f"{info['width']}x{info['height']} @ {info['fps']:.1f} fps"
    )
    print("Press q or Esc to exit.")

    window_name = "TrackingCar-Vision Camera Check"
    frame_count = 0
    last_time = time.time()
    display_fps = 0.0

    try:
        while True:
            frame = read_frame(capture)
            frame_count += 1

            now = time.time()
            elapsed = now - last_time
            if elapsed >= 1.0:
                display_fps = frame_count / elapsed
                frame_count = 0
                last_time = now

            cv2.putText(
                frame,
                f"Camera {camera_id} | FPS: {display_fps:.1f}",
                (12, 28),
                cv2.FONT_HERSHEY_SIMPLEX,
                0.8,
                (0, 255, 0),
                2,
                cv2.LINE_AA,
            )
            cv2.putText(
                frame,
                "Press q or Esc to exit",
                (12, frame.shape[0] - 16),
                cv2.FONT_HERSHEY_SIMPLEX,
                0.7,
                (0, 255, 255),
                2,
                cv2.LINE_AA,
            )

            cv2.imshow(window_name, frame)
            key = cv2.waitKey(1) & 0xFF
            if key in (ord("q"), 27):
                break
    finally:
        capture.release()
        cv2.destroyAllWindows()


if __name__ == "__main__":
    main()
