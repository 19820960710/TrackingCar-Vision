from settings import LASER_JUMP_MIN_PX, LASER_JUMP_RATIO
from vision.geometry.quadrilateral import distance


def jump_limit(target_rect):
    return max(LASER_JUMP_MIN_PX, LASER_JUMP_RATIO * max(target_rect[2], target_rect[3]))


def accepts(old_point, new_point, target_rect):
    return old_point is None or distance(old_point, new_point) <= jump_limit(target_rect)
