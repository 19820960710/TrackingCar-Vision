"""Dictionary-based data contract compatible with MaixPy environments."""

MEASUREMENT_FIELDS = (
    "kind", "valid", "updated", "predicted", "lost_hold",
    "x", "y", "rect", "corners", "confidence",
    "timestamp_ms", "age_ms", "reason", "mode", "color",
)


def new_measurement(kind):
    return {
        "kind": kind,
        "valid": False,
        "updated": False,
        "predicted": False,
        "lost_hold": False,
        "x": 0,
        "y": 0,
        "rect": None,
        "corners": None,
        "confidence": 0,
        "timestamp_ms": 0,
        "age_ms": 0,
        "reason": "INIT",
        "mode": "NONE",
        "color": "unknown",
    }


def normalize_measurement(kind, measurement=None):
    result = new_measurement(kind)
    if measurement is None:
        return result
    if not isinstance(measurement, dict):
        raise TypeError("measurement must be a dict or None")
    unknown = set(measurement) - set(MEASUREMENT_FIELDS)
    if unknown:
        raise KeyError("unsupported measurement fields: %s" % sorted(unknown))
    result.update(measurement)
    result["kind"] = kind
    result["age_ms"] = max(0, int(result["age_ms"]))
    result["confidence"] = max(0, min(100, int(result["confidence"])))
    return result


def new_observation(target=None, laser=None, frame_index=0, timestamp_ms=0):
    return {
        "target": normalize_measurement("target", target),
        "laser": normalize_measurement("laser", laser),
        "frame_index": int(frame_index),
        "timestamp_ms": int(timestamp_ms),
    }
