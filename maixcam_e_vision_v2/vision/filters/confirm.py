class FrameConfirm:
    def __init__(self, required_frames):
        self.required_frames = required_frames
        self.count = 0

    def update(self, matched):
        self.count = self.count + 1 if matched else 0
        return self.count >= self.required_frames
