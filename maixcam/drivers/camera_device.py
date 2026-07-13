from maix import camera

from settings import *
def create_camera():
    try:
        return camera.Camera(CAMERA_WIDTH, CAMERA_HEIGHT, fps=CAMERA_FPS, buff_num=CAMERA_BUFFER_NUM)
    except TypeError:
        pass

    try:
        return camera.Camera(CAMERA_WIDTH, CAMERA_HEIGHT, fps=CAMERA_FPS)
    except TypeError:
        pass

    try:
        return camera.Camera(CAMERA_WIDTH, CAMERA_HEIGHT, buff_num=CAMERA_BUFFER_NUM)
    except TypeError:
        print("camera fps/buff_num not supported, fallback to default camera")
        return camera.Camera(CAMERA_WIDTH, CAMERA_HEIGHT)


def camera_setting_enabled(value):
    return value is not None and value >= 0


def call_camera_methods(cam, method_names, value):
    for method_name in method_names:
        try:
            method = getattr(cam, method_name)
        except Exception:
            continue

        try:
            method(value)
            return True
        except Exception as err:
            print("camera %s failed: %s" % (method_name, err))

    return False


def set_manual_exposure_mode(cam):
    try:
        return cam.exp_mode(camera.AeMode.Manual)
    except Exception:
        pass

    try:
        return cam.exp_mode(1)
    except Exception as err:
        print("camera manual exposure mode failed: %s" % err)
        return None


def apply_camera_tuning(cam):
    if camera_setting_enabled(CAMERA_EXPOSURE) or camera_setting_enabled(CAMERA_GAIN):
        set_manual_exposure_mode(cam)
    if camera_setting_enabled(CAMERA_EXPOSURE):
        actual = call_camera_methods(cam, ["exposure"], CAMERA_EXPOSURE)
        if actual:
            print("camera exposure set: %d" % CAMERA_EXPOSURE)
    if camera_setting_enabled(CAMERA_GAIN):
        actual = call_camera_methods(cam, ["gain"], CAMERA_GAIN)
        if actual:
            print("camera gain set: %d" % CAMERA_GAIN)
    if camera_setting_enabled(CAMERA_CONTRAST):
        call_camera_methods(cam, ["constrast", "contrast"], CAMERA_CONTRAST)

    if CAMERA_WB_GAIN:
        try:
            cam.awb_mode(camera.AwbMode.Manual)
        except Exception as err:
            print("camera manual awb failed: %s" % err)
        call_camera_methods(cam, ["set_wb_gain"], CAMERA_WB_GAIN)


def skip_camera_startup_frames(cam):
    if CAMERA_SKIP_FRAMES <= 0:
        return

    try:
        cam.skip_frames(CAMERA_SKIP_FRAMES)
        return
    except Exception:
        pass

    for _ in range(CAMERA_SKIP_FRAMES):
        try:
            cam.read()
        except Exception:
            return
