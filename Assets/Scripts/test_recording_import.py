# SPDX-License-Identifier: MIT
import csv
import json
from pathlib import Path
import tempfile
import unittest
from Assets.Scripts.recording_import import import_recordings


class RecordingImportTests(unittest.TestCase):
    def test_original_inputs_and_engine_observations_are_preserved_separately(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source = root / "source" / "session_20261113-test"
            source.mkdir(parents=True)
            raw = json.dumps({"BallData": {"Speed": 67.47, "VLA": -0.14, "HLA": 16.22,
                                          "BackSpin": 1406, "SideSpin": 4599, "TotalSpin": 4809, "SpinAxis": 72.99},
                              "UOpenGolf": {"Club": "gw", "Engine": {"TotalYd": 129.22}}})
            (source / "shot.json").write_text(raw, encoding="utf-8")
            target = root / "library"
            self.assertEqual(import_recordings(source.parent, target), 1)
            with (target / "shots.csv").open(newline="", encoding="utf-8") as handle:
                row = next(csv.DictReader(handle))
            self.assertEqual(row["vla_deg"], "-0.14")
            self.assertEqual(row["total_spin_rpm"], "4809")
            self.assertEqual(row["total_yd"], "")
            self.assertEqual(row["carry_yd"], "")
            # The game's selected club is kept only as an unverified label.
            self.assertEqual(row["reported_club"], "gw")
            self.assertNotIn("club", row)
            self.assertEqual((target / "recordings" / source.name / "shot.json").read_text(encoding="utf-8"), raw)


if __name__ == "__main__":
    unittest.main()
