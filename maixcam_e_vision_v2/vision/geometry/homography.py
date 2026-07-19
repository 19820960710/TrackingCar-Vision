"""Small pure-Python homography solver and coordinate projection helpers."""

EPSILON = 1e-10


def _solve_linear_system(matrix, vector):
    """Solve an n x n system by Gauss-Jordan elimination with pivoting."""
    size = len(vector)
    augmented = [list(matrix[row]) + [float(vector[row])] for row in range(size)]
    for column in range(size):
        pivot = max(range(column, size), key=lambda row: abs(augmented[row][column]))
        if abs(augmented[pivot][column]) <= EPSILON:
            raise ValueError("point configuration is degenerate")
        augmented[column], augmented[pivot] = augmented[pivot], augmented[column]
        scale = augmented[column][column]
        augmented[column] = [value / scale for value in augmented[column]]
        for row in range(size):
            if row == column:
                continue
            factor = augmented[row][column]
            if factor:
                augmented[row] = [augmented[row][index] - factor * augmented[column][index] for index in range(size + 1)]
    return [augmented[row][-1] for row in range(size)]


def compute(source_points, destination_points):
    """Compute a 3x3 matrix mapping four source points onto four destinations."""
    if len(source_points) != 4 or len(destination_points) != 4:
        raise ValueError("homography requires exactly four source and destination points")
    matrix = []
    vector = []
    for source, destination in zip(source_points, destination_points):
        x, y = float(source[0]), float(source[1])
        u, v = float(destination[0]), float(destination[1])
        matrix.append([x, y, 1.0, 0.0, 0.0, 0.0, -u * x, -u * y])
        vector.append(u)
        matrix.append([0.0, 0.0, 0.0, x, y, 1.0, -v * x, -v * y])
        vector.append(v)
    h = _solve_linear_system(matrix, vector)
    return ((h[0], h[1], h[2]), (h[3], h[4], h[5]), (h[6], h[7], 1.0))


def project(matrix, point):
    """Project one 2D point through a 3x3 homography matrix."""
    x, y = float(point[0]), float(point[1])
    denominator = matrix[2][0] * x + matrix[2][1] * y + matrix[2][2]
    if abs(denominator) <= EPSILON:
        raise ValueError("point projects to infinity")
    return (
        (matrix[0][0] * x + matrix[0][1] * y + matrix[0][2]) / denominator,
        (matrix[1][0] * x + matrix[1][1] * y + matrix[1][2]) / denominator,
    )


def invert(matrix):
    """Return the inverse of a non-singular 3x3 matrix."""
    a, b, c = matrix[0]
    d, e, f = matrix[1]
    g, h, i = matrix[2]
    determinant = a * (e * i - f * h) - b * (d * i - f * g) + c * (d * h - e * g)
    if abs(determinant) <= EPSILON:
        raise ValueError("homography matrix is singular")
    return (
        ((e * i - f * h) / determinant, (c * h - b * i) / determinant, (b * f - c * e) / determinant),
        ((f * g - d * i) / determinant, (a * i - c * g) / determinant, (c * d - a * f) / determinant),
        ((d * h - e * g) / determinant, (b * g - a * h) / determinant, (a * e - b * d) / determinant),
    )
