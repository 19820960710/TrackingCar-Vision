"""V2 camera-only preview. No target, laser or motor logic."""

from drivers.camera_device import apply_initial_tuning, create_camera, warmup


def main():
    from maix import app, display
    camera_device = create_camera()
    apply_initial_tuning(camera_device)
    warmup(camera_device)
    screen = display.Display()
    print("V2 camera preview ready; close the application to exit")
    while not app.need_exit():
        screen.show(camera_device.read())


if __name__ == "__main__":
    main()
