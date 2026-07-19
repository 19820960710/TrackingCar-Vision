"""Score LAB laser candidates using circularity, density and area."""


def score(candidate):
    if not candidate:
        return 0
    return candidate.get("score", 0)


def candidate_score(area, circularity, density, min_area, max_area):
    area_center = (min_area + max_area) * 0.5
    area_span = max(1.0, area_center - min_area)
    area_score = max(0.0, 1.0 - abs(area - area_center) / area_span)
    return max(0.0, min(1.0, 0.20 * area_score + 0.45 * circularity + 0.35 * density))
