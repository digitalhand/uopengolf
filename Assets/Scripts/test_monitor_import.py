# SPDX-License-Identifier: MIT
"""Checks for the launch-monitor export importer."""
import csv
from pathlib import Path
import tempfile
import unittest

import Assets.Scripts.monitor_import as monitor_import
from Assets.Scripts.data_paths import local_library_dir

HEADER = ("Club,Index,Ball Speed(mph),Launch Direction,Launch Angle,Spin Rate,Spin Axis,Back Spin,Side Spin,Apex(yd),"
          "Carry(yd),Total(yd),Offline(yd),Landing Angle,Club Path,Face Angle,Attack Angle,Dynamic Loft,ImpactHorizontal,"
          "ImpactVertical,ClubSpeed,SmashFactor")
EXPORT = "\n".join([
    "Dates,21:32 13 Nov. 2026,Place,Driving Ranges",
    "",
    HEADER,
    "3 Hybrid,0,108.4,L7.1,12.2,3667,0.0,3667,0,46.4,150.5,166.4,L17.1,30.1,L0.0,L0.0,0.0,0.0,0.0,14.6,0.0,0.0",
    "3 Hybrid,14,92.9,R4.0,16.7,5064,R7.9,5016,R697,46.9,121.8,131.7,R13.4,34.2,R0.2,R4.7,-6.6,23.9,0.0,14.6,0.0,0.0",
    ",Average,100.6,L1.5,14.4,4365,R3.9,4341,R348,46.6,136.1,149.0,L1.8,32.1,R0.1,R2.3,-3.3,11.9",
    ",Deviation,7.7,5.5,2.2,698,3.9,674,348.5,0.2,14.4,17.3,15.2,2.0,0.1,2.4,3.3,11.9",
    "6 Iron,15,70.8,R6.9,-1.8,1273,0.0,1273,0,0.0,0.0,90.1,0.0,0.0,L0.0,L0.0,0.0,0.0,0.0,14.6,0.0,0.0",
]) + "\n"


class MonitorImportTests(unittest.TestCase):
    def test_signed_values(self):
        self.assertEqual(monitor_import.signed("L7.1"), -7.1)
        self.assertEqual(monitor_import.signed("R8.4"), 8.4)
        self.assertEqual(monitor_import.signed("0.0"), 0.0)
        self.assertEqual(monitor_import.signed("-6.6"), -6.6)

    def test_rows_skip_summaries_and_keep_signs(self):
        shots = monitor_import.parse_export(EXPORT)
        self.assertEqual([shot["shot_name"] for shot in shots], ["3hybrid_00", "3hybrid_14", "6iron_15"])
        fade = shots[1]
        self.assertEqual(fade["reported_club"], "3 Hybrid")
        self.assertEqual((fade["hla_deg"], fade["spin_axis_deg"], fade["sidespin_rpm"], fade["offline_yd"]), (4.0, 7.9, 697.0, 13.4))
        self.assertEqual(shots[0]["hla_deg"], -7.1)
        self.assertEqual(shots[0]["offline_yd"], -17.1)
        # The column the export labels Apex(yd) holds feet.
        self.assertEqual(shots[0]["apex_ft"], 46.4)
        self.assertAlmostEqual(shots[0]["rollout_yd"], 15.9)
        ground = shots[2]
        self.assertEqual((ground["vla_deg"], ground["carry_yd"], ground["rollout_yd"]), (-1.8, 0.0, 90.1))

    def test_session_date(self):
        self.assertEqual(monitor_import.session_date(EXPORT.splitlines()), "20261113")
        self.assertIsNone(monitor_import.session_date(["no date here"]))

    def test_rejects_an_export_without_its_columns(self):
        with self.assertRaises(ValueError):
            monitor_import.parse_export("Club,Index,Ball Speed(mph)\n3 Hybrid,0,100\n")
        with self.assertRaises(ValueError):
            monitor_import.parse_export("nothing to see\n")

    def test_local_sessions_match_their_exports(self):
        """Every local session imported from an export regenerates byte for byte (skipped without local data)."""
        sessions = sorted(path.parent for path in local_library_dir().glob("*/" + monitor_import.EXPORT_NAME))
        if not sessions:
            self.skipTest(f"no imported exports in {local_library_dir()}")
        for session in sessions:
            with self.subTest(session=session.name), tempfile.TemporaryDirectory() as directory:
                regenerated = Path(directory) / monitor_import.SHOTS_NAME
                shots = monitor_import.parse_export((session / monitor_import.EXPORT_NAME).read_text(encoding="utf-8-sig"))
                monitor_import.write_shots(shots, regenerated)
                self.assertEqual(regenerated.read_bytes(), (session / monitor_import.SHOTS_NAME).read_bytes())

    def test_import_copies_the_export_and_writes_shots(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            export = root / "source.csv"
            export.write_text(EXPORT, encoding="utf-8")
            library = root / "ShotLibrary"
            self.assertEqual(monitor_import.main([str(export), "--library", str(library)]), 0)
            session = library / "export_20261113"
            self.assertEqual((session / monitor_import.EXPORT_NAME).read_text(encoding="utf-8"), EXPORT)
            with (session / monitor_import.SHOTS_NAME).open(newline="", encoding="utf-8") as handle:
                rows = list(csv.DictReader(handle))
            self.assertEqual(len(rows), 3)
            self.assertEqual(list(rows[0]), monitor_import.SHOTS_FIELDS)
            self.assertEqual(rows[1]["spin_axis_deg"], "7.90")
            self.assertEqual(rows[1]["backspin_rpm"], "5016")
            self.assertEqual(rows[2]["vla_deg"], "-1.80")


if __name__ == "__main__":
    unittest.main()
