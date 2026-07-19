"""Compatibility protocol for the existing MSPM0 receiver."""

from protocol.safety import is_fresh


def encode(target, laser, max_age_ms=100):
    target = target or {}
    laser = laser or {}
    target_valid = is_fresh(target, max_age_ms)
    laser_valid = is_fresh(laser, max_age_ms)
    if target_valid and laser_valid:
        dx = int(target["x"] - laser["x"])
        dy = int(target["y"] - laser["y"])
        return "AIM,1,%d,%d,%d,%d,%d,%d,%s,%s\n" % (dx, dy, target["x"], target["y"], laser["x"], laser["y"], target.get("mode", "V2"), laser.get("color", "blue_violet"))
    if target_valid:
        return "AIM,0,0,0,%d,%d,0,0,%s,NO_LASER\n" % (target["x"], target["y"], target.get("mode", "V2"))
    return "AIM,0,0,0,0,0,0,0,LOST,LOST\n"
