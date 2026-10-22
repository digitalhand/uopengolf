# SPDX-License-Identifier: MIT
"""Engine-free checks for accepting and rejecting a packaged game's start log."""
from pathlib import Path
import tempfile
import unittest

from check_package_start import validate_start_log


INIT = "LogInit: Display: Starting Game.\n"
LOADED = "[2026.10.17-12.00.00:000][  0]LogLoad: Took 0.41 seconds to LoadMap(/Game/UOpenGolf/Maps/UOpenGolf)\n"


class PackageStartLogTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.path = Path(self.directory.name) / "Game.log"

    def write(self, text):
        self.path.write_text(text, encoding="utf-8")

    def test_accepts_clean_start(self):
        self.write(INIT + LOADED)
        validate_start_log(self.path)

    def test_rejects_missing_map_marker(self):
        self.write(INIT)
        with self.assertRaisesRegex(ValueError, "LoadMap"):
            validate_start_log(self.path)

    def test_rejects_crash_markers(self):
        for marker in ("Fatal error", "Assertion failed", "Unhandled Exception", "appError called"):
            with self.subTest(marker=marker):
                self.write(INIT + LOADED + f"LogCore: Error: {marker}: something broke\n")
                with self.assertRaisesRegex(ValueError, marker):
                    validate_start_log(self.path)

    def test_rejects_other_map(self):
        self.write(INIT + "LogLoad: Took 0.41 seconds to LoadMap(/Game/Other)\n")
        with self.assertRaises(ValueError):
            validate_start_log(self.path)

    def test_rejects_missing_or_empty_file(self):
        with self.assertRaises(ValueError):
            validate_start_log(self.path)
        self.write("")
        with self.assertRaises(ValueError):
            validate_start_log(self.path)


if __name__ == "__main__":
    unittest.main()
