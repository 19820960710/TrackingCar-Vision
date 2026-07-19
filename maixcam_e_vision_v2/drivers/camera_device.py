"""MaixCAM camera setup and baseline tuning; no visual algorithm belongs here."""

from settings import CAMERA_BUFFER_NUM, CAMERA_EXPOSURE, CAMERA_FPS, CAMERA_GAIN, CAMERA_HEIGHT, CAMERA_WARMUP_FRAMES, CAMERA_WB_GAIN, CAMERA_WIDTH


def create_camera(width=None, height=None, fps=None):
    from maix import camera
    width = CAMERA_WIDTH if width is None else width
    height = CAMERA_HEIGHT if height is None else height
    fps = CAMERA_FPS if fps is None else fps
    try:
        return camera.Camera(width, height, fps=fps, buff_num=CAMERA_BUFFER_NUM)
    except TypeError:
        return camera.Camera(width, height, fps=fps)


def _call(camera_device, method_name, value):
    try:
        getattr(camera_device, method_name)(value)
        return True
    except Exception as error:
        print("camera %s failed: %s" % (method_name, error))
        return False


def apply_initial_tuning(camera_device):
    """Lock the provisional field profile; final values must be measured and recorded."""
    from maix import camera
    if CAMERA_EXPOSURE is not None or CAMERA_GAIN is not None:
        try:
            camera_device.exp_mode(camera.AeMode.Manual)
        except Exception as error:
            print("camera manual exposure mode failed: %s" % error)
    if CAMERA_EXPOSURE is not None:
        _call(camera_device, "exposure", CAMERA_EXPOSURE)
    if CAMERA_GAIN is not None:
        _call(camera_device, "gain", CAMERA_GAIN)
    if CAMERA_WB_GAIN:
        try:
            camera_device.awb_mode(camera.AwbMode.Manual)
        except Exception as error:
            print("camera manual white balance failed: %s" % error)
        _call(camera_device, "set_wb_gain", CAMERA_WB_GAIN)


def warmup(camera_device, frames=None):
    frames = CAMERA_WARMUP_FRAMES if frames is None else frames
    try:
        camera_device.skip_frames(frames)
        return
    except Exception:
        pass
    for _ in range(frames):
        camera_device.read()
