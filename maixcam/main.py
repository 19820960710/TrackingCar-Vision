from maix import app, camera, display, image, time

from config import (
    CAMERA_HEIGHT,
    CAMERA_WIDTH,
    CROSSHAIR_SIZE,
    PRINT_FPS,
    SHOW_CENTER_GUIDE,
    SHOW_FPS,
)


def draw_debug_overlay(img, fps):
    center_x = CAMERA_WIDTH // 2
    center_y = CAMERA_HEIGHT // 2

    if SHOW_CENTER_GUIDE:
        img.draw_line(
            center_x - CROSSHAIR_SIZE,
            center_y,
            center_x + CROSSHAIR_SIZE,
            center_y,
            image.COLOR_GREEN,
        )
        img.draw_line(
            center_x,
            center_y - CROSSHAIR_SIZE,
            center_x,
            center_y + CROSSHAIR_SIZE,
            image.COLOR_GREEN,
        )
        img.draw_circle(center_x, center_y, 4, image.COLOR_RED, 1)

    img.draw_string(8, 8, "TrackingCar Vision", image.COLOR_GREEN)
    if SHOW_FPS:
        img.draw_string(8, 28, "FPS: %.1f" % fps, image.COLOR_GREEN)
    img.draw_string(8, 48, "size: %dx%d" % (CAMERA_WIDTH, CAMERA_HEIGHT), image.COLOR_GREEN)
    img.draw_string(8, 68, "center: (%d,%d)" % (center_x, center_y), image.COLOR_GREEN)
    img.draw_string(8, 88, "x->right  y->down", image.COLOR_GREEN)


def process_frame(img, fps):
    """Stage-1 debug view.

    Later stages will add target detection, laser spot detection, and serial
    output here. For now, draw the coordinate guide used by all later logic.
    """
    draw_debug_overlay(img, fps)
    return img


def main():
    cam = camera.Camera(CAMERA_WIDTH, CAMERA_HEIGHT)
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
