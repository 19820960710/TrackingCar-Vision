from vision.geometry.quadrilateral import distance


def accepts_point(old_point, new_point, limit):
    return old_point is None or distance(old_point, new_point) <= limit
