import unittest

from app.models import MEASUREMENT_FIELDS, new_measurement, new_observation, normalize_measurement


class ModelContractTest(unittest.TestCase):
    def test_default_measurement_has_every_contract_field(self):
        self.assertEqual(set(new_measurement("target")), set(MEASUREMENT_FIELDS))

    def test_normalize_fills_defaults_and_clamps_ranges(self):
        target = normalize_measurement("target", {"valid": True, "x": 12, "confidence": 150, "age_ms": -1})
        self.assertTrue(target["valid"])
        self.assertEqual(target["confidence"], 100)
        self.assertEqual(target["age_ms"], 0)
        self.assertEqual(target["kind"], "target")

    def test_unknown_field_is_rejected(self):
        with self.assertRaises(KeyError):
            normalize_measurement("target", {"unexpected": 1})

    def test_observation_normalizes_both_measurements(self):
        observation = new_observation({"valid": True}, {"valid": True, "color": "blue_violet"}, 7, 123)
        self.assertEqual(observation["target"]["kind"], "target")
        self.assertEqual(observation["laser"]["kind"], "laser")
        self.assertEqual(observation["frame_index"], 7)


if __name__ == "__main__":
    unittest.main()
