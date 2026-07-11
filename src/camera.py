import cv2


def open_camera(camera_id=0, width=640, height=480, fps=30):
    """Open a camera device with basic OpenCV settings."""
    capture = cv2.VideoCapture(camera_id, cv2.CAP_DSHOW)
    if not capture.isOpened():
        capture.release()
        capture = cv2.VideoCapture(camera_id)

    if not capture.isOpened():
        raise RuntimeError(f"Cannot open camera: {camera_id}")

    capture.set(cv2.CAP_PROP_FRAME_WIDTH, width)
    capture.set(cv2.CAP_PROP_FRAME_HEIGHT, height)
    capture.set(cv2.CAP_PROP_FPS, fps)
    return capture


def read_frame(capture):
    """Read one frame from an opened camera."""
    ok, frame = capture.read()
    if not ok or frame is None:
        raise RuntimeError("Cannot read frame from camera")
    return frame


def get_camera_info(capture):
    """Return the actual camera properties reported by OpenCV."""
    return {
        "width": int(capture.get(cv2.CAP_PROP_FRAME_WIDTH)),
        "height": int(capture.get(cv2.CAP_PROP_FRAME_HEIGHT)),
        "fps": capture.get(cv2.CAP_PROP_FPS),
    }
