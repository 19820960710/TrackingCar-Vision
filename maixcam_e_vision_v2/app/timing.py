"""Monotonic microsecond timing and bounded P95 statistics."""

import time


def now_us():
    if hasattr(time, "ticks_us"):
        return time.ticks_us()
    return int(time.time() * 1000000)


def now_ms():
    if hasattr(time, "ticks_ms"):
        return time.ticks_ms()
    return int(time.time() * 1000)


def elapsed_us(newer, older):
    if hasattr(time, "ticks_diff"):
        return time.ticks_diff(newer, older)
    return newer - older


def percentile(values, fraction):
    if not values:
        return 0
    ordered = sorted(values)
    return ordered[min(len(ordered) - 1, int((len(ordered) - 1) * fraction))]


class PerformanceStats:
    def __init__(self, capacity=120):
        self.capacity = max(0, int(capacity))
        self.samples = {}
        self._next_index = {}

    def add(self, name, value_us):
        if self.capacity <= 0:
            return
        values = self.samples.setdefault(name, [])
        value_us = max(0, int(value_us))
        if len(values) < self.capacity:
            values.append(value_us)
            return
        index = self._next_index.get(name, 0)
        values[index] = value_us
        self._next_index[name] = (index + 1) % self.capacity

    def summary(self, name):
        values = self.samples.get(name, [])
        return {"count": len(values), "p50_us": percentile(values, 0.50), "p95_us": percentile(values, 0.95)}

    def snapshot(self):
        return {name: self.summary(name) for name in self.samples}
