import unittest

from app.models import new_measurement
from app.state_machine import FALLBACK, LOST, RECAPTURE, TRACK, TargetRecoveryStateMachine, next_mode
from vision.target.ai_recapture import AiRecapture


def valid_target(timestamp_ms=0):
    result = new_measurement("target")
    result.update({
        "valid": True, "updated": True, "x": 40, "y": 30,
        "rect": (20, 10, 40, 40), "corners": ((20, 10), (60, 10), (60, 50), (20, 50)),
        "confidence": 88, "timestamp_ms": timestamp_ms, "reason": "OK", "mode": "TRADITIONAL_ROI",
    })
    return result


def invalid_target(reason, timestamp_ms=0):
    result = new_measurement("target")
    result.update({"reason": reason, "timestamp_ms": timestamp_ms, "mode": "TRADITIONAL_ROI"})
    return result


class QueueRefiner:
    def __init__(self, results):
        self.results = list(results)
        self.rois = []

    def __call__(self, _frame, roi, timestamp_ms):
        self.rois.append(roi)
        result = self.results.pop(0)
        result["timestamp_ms"] = timestamp_ms
        return result


class StateMachineTest(unittest.TestCase):
    def test_valid_target_tracks(self):
        self.assertEqual(next_mode(RECAPTURE, True, 0, True, 3), TRACK)

    def test_model_recapture(self):
        self.assertEqual(next_mode(TRACK, False, 3, True, 3), RECAPTURE)

    def test_fallback_without_model(self):
        self.assertEqual(next_mode(RECAPTURE, False, 3, False, 3), FALLBACK)

    def test_roi_fail_limit_clears_old_roi_then_ai_refines_new_roi(self):
        refiner = QueueRefiner([invalid_target("NO_VALID_QUAD"), invalid_target("NO_VALID_QUAD"), invalid_target("NO_VALID_QUAD"), valid_target()])
        ai = AiRecapture(lambda _frame: {"valid": True, "rect": (100, 40, 80, 80), "confidence": 74})
        machine = TargetRecoveryStateMachine(refiner=refiner, ai_recapture=ai, roi_fail_limit=3)
        machine.active_roi = (1, 2, 3, 4)
        frame = type("Frame", (), {"shape": (120, 200, 3)})()
        for timestamp in (10, 20):
            target, status = machine.step(frame, timestamp)
            self.assertFalse(target["valid"])
            self.assertEqual(status["mode"], TRACK)
        target, status = machine.step(frame, 30)
        self.assertTrue(target["valid"])
        self.assertEqual(status["mode"], TRACK)
        self.assertEqual(status["reason"], "AI_GLOBAL_REFINED")
        self.assertEqual(status["timestamp_ms"], 30)
        self.assertEqual(status["confidence"], 88)
        self.assertNotEqual(refiner.rois[-1], (1, 2, 3, 4))

    def test_model_unavailable_uses_classical_proposal_then_refines(self):
        fallback = lambda _frame, timestamp: {"valid": True, "rect": (30, 30, 60, 60), "confidence": 51, "reason": "BLACK_RECT_GLOBAL_PROPOSAL", "timestamp_ms": timestamp}
        machine = TargetRecoveryStateMachine(refiner=QueueRefiner([valid_target()]), fallback_proposer=fallback)
        frame = type("Frame", (), {"shape": (120, 200, 3)})()
        target, status = machine.step(frame, 40)
        self.assertTrue(target["valid"])
        self.assertEqual(status["mode"], TRACK)
        self.assertEqual(status["reason"], "CLASSICAL_GLOBAL_REFINED")
        self.assertEqual(status["timestamp_ms"], 40)

    def test_total_failure_enters_lost_with_reason_and_timestamp(self):
        fallback = lambda _frame, timestamp: {"valid": False, "rect": None, "confidence": 0, "reason": "NO_BLACK_FRAME", "timestamp_ms": timestamp}
        machine = TargetRecoveryStateMachine(refiner=QueueRefiner([]), fallback_proposer=fallback)
        frame = type("Frame", (), {"shape": (120, 200, 3)})()
        target, status = machine.step(frame, 50)
        self.assertFalse(target["valid"])
        self.assertEqual(status["mode"], LOST)
        self.assertEqual(status["reason"], "NO_BLACK_FRAME")
        self.assertEqual(status["timestamp_ms"], 50)
        self.assertEqual(status["confidence"], 0)
        self.assertIsNone(status["active_roi"])


if __name__ == "__main__":
    unittest.main()
