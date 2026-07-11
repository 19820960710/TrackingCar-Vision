from maix import app, camera, display, image, time

CAMERA_WIDTH = 640
CAMERA_HEIGHT = 480
SHOW_FPS = True
PRINT_FPS = True


def main():
    cam = camera.Camera(CAMERA_WIDTH, CAMERA_HEIGHT)
    disp = display.Display()

    while not app.need_exit():
        img = cam.read()
        fps = time.fps()

        if SHOW_FPS:
            img.draw_string(8, 8, "FPS: %.1f" % fps, image.COLOR_GREEN)
            img.draw_string(8, 28, "%dx%d" % (CAMERA_WIDTH, CAMERA_HEIGHT), image.COLOR_GREEN)

        disp.show(img)

        if PRINT_FPS:
            frame_ms = 1000 / fps if fps > 0 else 0
            print("frame: %.2f ms, fps: %.2f" % (frame_ms, fps))


if __name__ == "__main__":
    main()
