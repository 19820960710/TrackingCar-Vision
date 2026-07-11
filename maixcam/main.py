from maix import app, camera, display, image, time

from config import CAMERA_HEIGHT, CAMERA_WIDTH, PRINT_FPS, SHOW_FPS


def process_frame(img):
    """Stage-1 placeholder.

    Later stages will add target detection, laser spot detection, and serial
    output here. For now, keep the frame unchanged and verify camera preview.
    """
    return img


def main():
    cam = camera.Camera(CAMERA_WIDTH, CAMERA_HEIGHT)
    disp = display.Display()

    while not app.need_exit():
        img = cam.read()
        img = process_frame(img)
        fps = time.fps()

        if SHOW_FPS:
            img.draw_string(8, 8, "TrackingCar Vision", image.COLOR_GREEN)
            img.draw_string(8, 28, "FPS: %.1f" % fps, image.COLOR_GREEN)

        disp.show(img)

        if PRINT_FPS:
            print("fps: %.2f" % fps)


if __name__ == "__main__":
    main()
