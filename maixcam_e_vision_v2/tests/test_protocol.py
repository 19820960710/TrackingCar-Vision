import unittest

from main import encode_ascii_for_mspm0
from protocol.ascii_aim import encode
from protocol.crc16 import crc16_ibm
from settings import MSPM0_STALE_MS


class ProtocolTest(unittest.TestCase):
    def test_aim(self):
        target = {"valid": True, "x": 244, "y": 168, "mode": "perspective"}
        laser = {"valid": True, "x": 256, "y": 160, "color": "blue_violet"}
        self.assertEqual(encode(target, laser), "AIM,1,-12,8,244,168,256,160,perspective,blue_violet\n")

    def test_stale_target_is_lost(self):
        target = {"valid": True, "x": 244, "y": 168, "age_ms": 101}
        laser = {"valid": True, "x": 256, "y": 160}
        self.assertEqual(encode(target, laser), "AIM,0,0,0,0,0,0,0,LOST,LOST\n")

    def test_runtime_ascii_adapter_uses_configured_freshness_boundary(self):
        target = {"valid": True, "x": 244, "y": 168, "age_ms": MSPM0_STALE_MS}
        laser = {"valid": True, "x": 256, "y": 160, "age_ms": 0}
        self.assertTrue(encode_ascii_for_mspm0(target, laser).startswith("AIM,1,"))

        target["age_ms"] = MSPM0_STALE_MS + 1
        self.assertEqual(
            encode_ascii_for_mspm0(target, laser),
            "AIM,0,0,0,0,0,0,0,LOST,LOST\n",
        )
    def test_crc(self):
        self.assertEqual(crc16_ibm(b"123456789"), 0x4B37)


if __name__ == "__main__":
    unittest.main()
