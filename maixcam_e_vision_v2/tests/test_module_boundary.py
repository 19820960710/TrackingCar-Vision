import pathlib
import unittest


class ModuleBoundaryTest(unittest.TestCase):
    def test_vision_does_not_import_hardware_or_protocol_layers(self):
        root = pathlib.Path(__file__).resolve().parents[1] / "vision"
        forbidden = ("from drivers", "import drivers", "from protocol", "import protocol", "from mspm0", "import mspm0")
        for source in root.rglob("*.py"):
            for line in source.read_text(encoding="utf-8").splitlines():
                stripped = line.strip()
                self.assertFalse(stripped.startswith(forbidden), "%s violates vision boundary: %s" % (source, stripped))


if __name__ == "__main__":
    unittest.main()
