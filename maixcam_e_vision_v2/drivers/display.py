"""Display output and on-screen diagnostics for MaixCAM."""

from app.timing import elapsed_us, now_us
from settings import DISPLAY_VERBOSE_STATUS, ENABLE_DEBUG_DRAW


def _colors():
    from maix import image
    return image


def _integer(value):
    return int(round(float(value)))


def _draw_cross(frame, x, y, radius, color, thickness=2):
    x = _integer(x)
    y = _integer(y)
    frame.draw_line(x - radius, y, x + radius, y, color, thickness)
    frame.draw_line(x, y - radius, x, y + radius, color, thickness)


def _draw_offset_line(frame, target, laser, colors):
    """Show the same target-minus-laser relationship carried by AIM dx/dy."""
    if not target or not laser or not target.get("valid") or not laser.get("valid"):
        return
    frame.draw_line(
        _integer(target.get("x", 0)), _integer(target.get("y", 0)),
        _integer(laser.get("x", 0)), _integer(laser.get("y", 0)),
        colors.COLOR_GREEN, 1,
    )


def _draw_target(frame, target, colors):
    if not target or not target.get("valid"):
        return
    color = colors.COLOR_YELLOW if target.get("predicted") else colors.COLOR_RED
    corners = target.get("corners")
    if corners and len(corners) >= 4:
        for index in range(4):
            first = corners[index]
            second = corners[(index + 1) % 4]
            frame.draw_line(
                _integer(first[0]), _integer(first[1]),
                _integer(second[0]), _integer(second[1]),
                color, 2,
            )
    else:
        rect = target.get("rect")
        if rect and rect[2] > 0 and rect[3] > 0:
            frame.draw_rect(
                _integer(rect[0]), _integer(rect[1]),
                _integer(rect[2]), _integer(rect[3]),
                color, 2,
            )
    cross_x, cross_y = target.get("x", 0), target.get("y", 0)
    # Keep the live overlay on raw geometry to avoid visible filter lag. UART control still uses target x/y.
    if not target.get("predicted") and corners:
        cross_x = sum(float(point[0]) for point in corners) / len(corners)
        cross_y = sum(float(point[1]) for point in corners) / len(corners)
    elif not target.get("predicted") and target.get("rect"):
        rect = target["rect"]
        cross_x = float(rect[0]) + float(rect[2]) * 0.5
        cross_y = float(rect[1]) + float(rect[3]) * 0.5
    _draw_cross(frame, cross_x, cross_y, 8, color, 2)


def _draw_laser(frame, laser, colors):
    if not laser or not laser.get("valid"):
        return
    x = _integer(laser.get("x", 0))
    y = _integer(laser.get("y", 0))
    _draw_cross(frame, x, y, 6, colors.COLOR_BLUE, 2)
    frame.draw_circle(x, y, 4, colors.COLOR_BLUE, 2)


def _target_text(observation):
    target = observation.get("target") or {}
    if target.get("valid"):
        state = "PRED" if target.get("predicted") else "LOCK"
        return "TARGET: %s %d%% (%d,%d)" % (
            state,
            int(target.get("confidence", 0)),
            _integer(target.get("x", 0)),
            _integer(target.get("y", 0)),
        )
    target_state = observation.get("target_state") or {}
    reason = target_state.get("reason") or target.get("reason") or "UNKNOWN"
    return "TARGET: LOST %s" % reason


def _laser_text(observation):
    laser = observation.get("laser") or {}
    if laser.get("valid"):
        return "LASER: LOCK (%d,%d)" % (
            _integer(laser.get("x", 0)),
            _integer(laser.get("y", 0)),
        )
    return "LASER: LOST %s" % (laser.get("reason") or "UNKNOWN")


def _p95_ms(observation, name):
    performance = observation.get("performance") or {}
    return float((performance.get(name) or {}).get("p95_us", 0)) / 1000.0


def draw_debug(frame, observation, enabled=False, fps=0.0, colors=None, verbose_status=True):
    if not enabled:
        return frame
    observation = observation or {}
    colors = colors or _colors()
    _draw_offset_line(frame, observation.get("target"), observation.get("laser"), colors)
    _draw_target(frame, observation.get("target"), colors)
    _draw_laser(frame, observation.get("laser"), colors)

    fps_color = colors.COLOR_GREEN if fps >= 24.0 else colors.COLOR_YELLOW if fps >= 15.0 else colors.COLOR_RED
    frame.draw_string(8, 8, "FPS: %.1f" % fps, fps_color)
    target = observation.get("target") or {}
    if verbose_status:
        frame.draw_string(8, 28, _target_text(observation), colors.COLOR_RED)
        frame.draw_string(8, 48, _laser_text(observation), colors.COLOR_BLUE)
        frame.draw_string(
            8, 68,
            "P95 ms T:%.1f F:%.1f U:%.1f" % (
                _p95_ms(observation, "target"),
                _p95_ms(observation, "frame_cpu"),
                _p95_ms(observation, "uart_write"),
            ),
            colors.COLOR_YELLOW,
        )
    elif not target.get("valid"):
        frame.draw_string(8, 28, _target_text(observation), colors.COLOR_RED)
    return frame


class DisplayDevice:
    def __init__(self, screen=None, debug_enabled=None, clock_us=None, colors=None, verbose_status=None):
        if screen is None:
            from maix import display
            screen = display.Display()
        self.screen = screen
        self.debug_enabled = ENABLE_DEBUG_DRAW if debug_enabled is None else debug_enabled
        self.verbose_status = DISPLAY_VERBOSE_STATUS if verbose_status is None else verbose_status
        self.clock_us = clock_us or now_us
        self.colors = colors
        self.last_frame_us = None
        self.fps = 0.0
        self.last_overlay_error = None

    def _update_fps(self):
        current = self.clock_us()
        if self.last_frame_us is not None:
            interval_us = elapsed_us(current, self.last_frame_us)
            if interval_us > 0:
                instant_fps = 1000000.0 / interval_us
                self.fps = instant_fps if self.fps <= 0.0 else self.fps * 0.8 + instant_fps * 0.2
        self.last_frame_us = current
        return self.fps

    def show(self, frame, observation):
        fps = self._update_fps()
        try:
            if self.colors is None:
                self.colors = _colors()
            rendered = draw_debug(frame, observation, self.debug_enabled, fps, self.colors, self.verbose_status)
            self.last_overlay_error = None
        except Exception as error:
            rendered = frame
            message = "%s: %s" % (type(error).__name__, error)
            if message != self.last_overlay_error:
                print("display overlay failed: %s" % message)
                self.last_overlay_error = message
        self.screen.show(rendered)
        return rendered


def create_display():
    return DisplayDevice()
