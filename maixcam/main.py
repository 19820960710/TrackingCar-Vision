from maix import app, display, time

from settings import PRINT_FPS
from app.pipeline import print_startup_config, process_frame
from drivers.camera_device import apply_camera_tuning, create_camera, skip_camera_startup_frames
from drivers.uart_output import init_uart_output

def main():
    print_startup_config()
    cam = create_camera()
    apply_camera_tuning(cam)
    skip_camera_startup_frames(cam)
    init_uart_output()
    disp = display.Display()

    while not app.need_exit():
        img = cam.read()
        fps = time.fps()
        img = process_frame(img, fps)

        disp.show(img)

        if PRINT_FPS:
            print("fps: %.2f" % fps)


if __name__ == "__main__":
    main()
