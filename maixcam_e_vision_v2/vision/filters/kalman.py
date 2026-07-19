"""Small 2D constant-velocity Kalman filter without NumPy dependency."""


class _AxisKalman:
    def __init__(self, process_noise=30.0, measurement_noise=9.0):
        self.process_noise = float(process_noise)
        self.measurement_noise = float(measurement_noise)
        self.position = 0.0
        self.velocity = 0.0
        self.p00, self.p01, self.p10, self.p11 = 1.0, 0.0, 0.0, 1.0

    def predict(self, dt):
        self.position += self.velocity * dt
        q = self.process_noise
        self.p00 = self.p00 + dt * (self.p10 + self.p01) + dt * dt * self.p11 + q * dt * dt
        self.p01 = self.p01 + dt * self.p11
        self.p10 = self.p10 + dt * self.p11
        self.p11 = self.p11 + q * dt

    def correct(self, value):
        residual = value - self.position
        innovation = self.p00 + self.measurement_noise
        k0 = self.p00 / innovation
        k1 = self.p10 / innovation
        self.position += k0 * residual
        self.velocity += k1 * residual
        p00, p01, p10, p11 = self.p00, self.p01, self.p10, self.p11
        self.p00 = (1.0 - k0) * p00
        self.p01 = (1.0 - k0) * p01
        self.p10 = p10 - k1 * p00
        self.p11 = p11 - k1 * p01


class ConstantVelocityKalman:
    """Track x/y and predict short occlusions from timestamps in milliseconds."""

    def __init__(self, process_noise=30.0, measurement_noise=9.0):
        self.x_axis = _AxisKalman(process_noise, measurement_noise)
        self.y_axis = _AxisKalman(process_noise, measurement_noise)
        self.ready = False
        self.timestamp_ms = 0

    def reset(self):
        self.ready = False
        self.timestamp_ms = 0

    def _advance(self, timestamp_ms):
        if not self.ready:
            return 0.0
        dt = max(0.0, min(0.2, (int(timestamp_ms) - self.timestamp_ms) / 1000.0))
        self.x_axis.predict(dt)
        self.y_axis.predict(dt)
        self.timestamp_ms = int(timestamp_ms)
        return dt

    def update(self, x, y, timestamp_ms):
        timestamp_ms = int(timestamp_ms)
        if not self.ready:
            self.x_axis.position, self.y_axis.position = float(x), float(y)
            self.x_axis.velocity, self.y_axis.velocity = 0.0, 0.0
            self.timestamp_ms = timestamp_ms
            self.ready = True
            return self.x_axis.position, self.y_axis.position
        self._advance(timestamp_ms)
        self.x_axis.correct(float(x))
        self.y_axis.correct(float(y))
        return self.x_axis.position, self.y_axis.position

    def predict(self, timestamp_ms):
        if not self.ready:
            return None
        self._advance(timestamp_ms)
        return self.x_axis.position, self.y_axis.position
