"""Protocol-level freshness rules shared by ASCII and future binary frames."""


def is_fresh(measurement, max_age_ms):
    if not measurement or not measurement.get("valid"):
        return False
    if measurement.get("lost_hold"):
        return False
    return int(measurement.get("age_ms", 0)) <= max_age_ms
