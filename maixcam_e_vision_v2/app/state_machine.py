"""Target tracking/recovery state transitions with explicit status evidence."""

from app.models import new_measurement
from vision.target.ai_recapture import AiRecapture
from vision.target.global_proposal import propose_black_frame
from vision.target.roi import tracking_roi
from vision.target.roi_refine import refine

BOOT = "BOOT"
TRACK = "TRACK_ROI_REFINE"
RECAPTURE = "AI_GLOBAL_RECAPTURE"
FALLBACK = "CLASSICAL_GLOBAL_PROPOSAL"
LOST = "LOST"


def next_mode(current, target_valid, roi_fail_count, model_available, roi_fail_limit):
    if target_valid:
        return TRACK
    if current == TRACK and roi_fail_count < roi_fail_limit:
        return TRACK
    if model_available:
        return RECAPTURE
    if current != FALLBACK:
        return FALLBACK
    return LOST


def _frame_size(frame):
    if hasattr(frame, "shape"):
        return frame.shape[1], frame.shape[0]
    return frame.width(), frame.height()


class TargetRecoveryStateMachine:
    """Own the active ROI and guarantee it is cleared before global recovery."""

    def __init__(self, refiner=refine, ai_recapture=None, fallback_proposer=propose_black_frame, roi_fail_limit=3):
        self.refiner = refiner
        self.ai_recapture = ai_recapture or AiRecapture()
        self.fallback_proposer = fallback_proposer
        self.roi_fail_limit = roi_fail_limit
        self.mode = BOOT
        self.active_roi = None
        self.roi_fail_count = 0
        self.last_reason = "BOOT"
        self.last_confidence = 0
        self.last_timestamp_ms = 0

    def _status(self, timestamp_ms, reason, confidence=0, transitioned=False):
        self.last_reason = reason
        self.last_confidence = max(0, min(100, int(confidence)))
        self.last_timestamp_ms = int(timestamp_ms)
        return {
            "mode": self.mode,
            "reason": reason,
            "confidence": self.last_confidence,
            "timestamp_ms": self.last_timestamp_ms,
            "roi_fail_count": self.roi_fail_count,
            "active_roi": self.active_roi,
            "transitioned": bool(transitioned),
        }

    def _transition(self, mode, timestamp_ms, reason, confidence=0):
        changed = self.mode != mode
        self.mode = mode
        return self._status(timestamp_ms, reason, confidence, changed)

    def _invalid(self, reason, timestamp_ms):
        result = new_measurement("target")
        result.update({"timestamp_ms": int(timestamp_ms), "reason": reason, "mode": self.mode})
        return result

    def _use_valid_refinement(self, target, frame, timestamp_ms, reason):
        width, height = _frame_size(frame)
        self.active_roi = tracking_roi(target["rect"], width, height)
        self.roi_fail_count = 0
        return target, self._transition(TRACK, timestamp_ms, reason, target["confidence"])

    def _refine_active_roi(self, frame, timestamp_ms):
        target = self.refiner(frame, self.active_roi, timestamp_ms)
        if target.get("valid"):
            return self._use_valid_refinement(target, frame, timestamp_ms, "ROI_REFINE_OK")
        self.roi_fail_count += 1
        if self.roi_fail_count < self.roi_fail_limit:
            return target, self._transition(TRACK, timestamp_ms, "ROI_REFINE_FAILED_%s" % target.get("reason", "UNKNOWN"))
        self.active_roi = None
        return None, None

    def _proposal_to_refinement(self, proposal, frame, timestamp_ms, source):
        if not proposal.get("valid"):
            return None, None
        width, height = _frame_size(frame)
        self.active_roi = tracking_roi(proposal["rect"], width, height)
        target = self.refiner(frame, self.active_roi, timestamp_ms)
        if target.get("valid"):
            return self._use_valid_refinement(target, frame, timestamp_ms, "%s_REFINED" % source)
        self.active_roi = None
        self.roi_fail_count = 0
        return self._invalid("%s_REFINE_FAILED_%s" % (source, target.get("reason", "UNKNOWN")), timestamp_ms), None

    def step(self, frame, timestamp_ms):
        """Return `(target_measurement, state_status)` for the current frame only."""
        timestamp_ms = int(timestamp_ms)
        if self.active_roi is not None:
            target, status = self._refine_active_roi(frame, timestamp_ms)
            if target is not None:
                return target, status

        if self.ai_recapture.available():
            self._transition(RECAPTURE, timestamp_ms, "ROI_FAIL_LIMIT_REACHED" if self.roi_fail_count else "GLOBAL_RECAPTURE")
            proposal = self.ai_recapture.detect(frame, timestamp_ms)
            target, status = self._proposal_to_refinement(proposal, frame, timestamp_ms, "AI_GLOBAL")
            if target is not None:
                if status is None:
                    return target, self._transition(LOST, timestamp_ms, target["reason"], proposal.get("confidence", 0))
                return target, status
            ai_reason = proposal.get("reason", "MODEL_NO_TARGET")
        else:
            ai_reason = "MODEL_UNAVAILABLE"

        self._transition(FALLBACK, timestamp_ms, ai_reason)
        proposal = self.fallback_proposer(frame, timestamp_ms)
        target, status = self._proposal_to_refinement(proposal, frame, timestamp_ms, "CLASSICAL_GLOBAL")
        if target is not None:
            if status is None:
                return target, self._transition(LOST, timestamp_ms, target["reason"], proposal.get("confidence", 0))
            return target, status
        reason = proposal.get("reason", "NO_BLACK_FRAME")
        return self._invalid(reason, timestamp_ms), self._transition(LOST, timestamp_ms, reason, proposal.get("confidence", 0))
