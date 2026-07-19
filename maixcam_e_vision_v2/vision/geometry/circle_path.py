"""Standard-plane circle points and homography projection helpers."""

from math import cos, pi, sin

from vision.geometry.homography import project


def standard_circle(center_x_mm, center_y_mm, radius_mm, count=50):
    return [(center_x_mm + radius_mm * cos(2 * pi * index / count), center_y_mm + radius_mm * sin(2 * pi * index / count)) for index in range(count)]


def project_circle(plane_to_image, center_x_mm, center_y_mm, radius_mm, count=50):
    return tuple(project(plane_to_image, point) for point in standard_circle(center_x_mm, center_y_mm, radius_mm, count))
