"""Persistent bright-point background model collected while the laser is off."""

from vision.geometry.quadrilateral import distance


class StaticLightBackground:
    def __init__(self, match_radius=8, required_frames=25):
        self.match_radius = float(match_radius)
        self.required_frames = int(required_frames)
        self.points = []  # mutable records: {x, y, count}
        self.frames = 0

    def clear(self):
        self.points = []
        self.frames = 0

    def observe(self, candidates):
        """Add one laser-off frame of candidate points to the background model."""
        self.frames += 1
        for candidate in candidates:
            point = (candidate["x"], candidate["y"])
            matched = None
            for record in self.points:
                if distance(point, (record["x"], record["y"])) <= self.match_radius:
                    matched = record
                    break
            if matched is None:
                self.points.append({"x": point[0], "y": point[1], "count": 1})
            else:
                count = matched["count"]
                matched["x"] = (matched["x"] * count + point[0]) / (count + 1)
                matched["y"] = (matched["y"] * count + point[1]) / (count + 1)
                matched["count"] = count + 1

    def is_static(self, point):
        for record in self.points:
            if record["count"] >= self.required_frames and distance(point, (record["x"], record["y"])) <= self.match_radius:
                return True
        return False

    def ready(self):
        return self.frames >= self.required_frames
