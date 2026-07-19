"""Image-bound ROI clipping without camera dependencies."""

import math


def clip_roi(x, y, width, height, image_width, image_height):
    """Clip an ROI to image bounds and return integer (x, y, width, height).

    The input rectangle may have fractional coordinates.  Its covered pixels
    are preserved by flooring the start and ceiling the end.  Return None when
    the rectangle has no positive-area overlap with the image.
    """
    if image_width <= 0 or image_height <= 0 or width <= 0 or height <= 0:
        return None
    left = max(0, int(math.floor(x)))
    top = max(0, int(math.floor(y)))
    right = min(int(image_width), int(math.ceil(x + width)))
    bottom = min(int(image_height), int(math.ceil(y + height)))
    if right <= left or bottom <= top:
        return None
    return left, top, right - left, bottom - top
