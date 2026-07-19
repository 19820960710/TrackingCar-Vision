"""Small, allocation-light timing helpers for MaixCAM baseline tests."""


def percentile(values, fraction):
    if not values:
        return 0
    ordered = sorted(values)
    return ordered[min(len(ordered) - 1, int((len(ordered) - 1) * fraction))]


class TimingWindow:
    def __init__(self, capacity=60):
        self.capacity = capacity
        self.samples = {}

    def add(self, name, value_us):
        values = self.samples.setdefault(name, [])
        values.append(value_us)
        if len(values) > self.capacity:
            values.pop(0)

    def summary(self, name):
        values = self.samples.get(name, [])
        return {"count": len(values), "p50_us": percentile(values, 0.50), "p95_us": percentile(values, 0.95)}
