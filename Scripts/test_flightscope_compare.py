# SPDX-License-Identifier: MIT
"""Engine-free checks for the FlightScope comparison script."""
import contextlib
from datetime import datetime
import io
from pathlib import Path
import tempfile
import unittest

import flightscope_compare as compare_script


LIBRARY_HEADER = ("shot_name,filename,speed_mph,vla_deg,hla_deg,total_spin_rpm,spin_axis_deg,"
                  "backspin_rpm,sidespin_rpm,carry_yd,total_yd,rollout_yd,apex_ft\n")
RESULTS_HEADER = "session,shot_name,profile,carry_yd,total_yd,rollout_yd,apex_ft,hang_s,lateral_yd,at_rest\n"


class FlightScopeCompareTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)
        self.library = self.root / "ShotLibrary"
        self.results = self.root / "results.csv"
        self.history = self.root / "history"
        self.write(self.library / "session_5" / "fs.csv", LIBRARY_HEADER
                   + "shot_driver_42,shot_driver_42.json,136.11,8.71,-3.00,2154.9,-0.59,2154.8,-22.3,193.9,216.9,23.0,39.1\n"
                   + "shot_lw_12,shot_lw_12.json,27.92,32.89,2.39,4103.5,0.00,4103.5,0.0,0.0,0.0,0.0,0.0\n")
        # Standard shots record rollout as 0.0; roll is recomputed from total - carry.
        self.write(self.library / "standard" / "fs.csv", LIBRARY_HEADER
                   + "drive_test_shot,drive_test_shot.json,150.00,12.50,1.50,2335.0,-9.90,2300.0,-400.0,244,256,0.0,80.6\n")
        self.write_results(current=(196.9, 210.9, 40.1), historical=(190.9, 220.9, 37.1))

    @staticmethod
    def write(path, text):
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text, encoding="utf-8")

    def write_results(self, current, historical, drive=(248.4, 262.5, 82.2)):
        rows = [RESULTS_HEADER]
        for profile, (carry, total, apex) in (("current", current), ("historical", historical)):
            rows.append(f"session_5,shot_driver_42,{profile},{carry},{total},{total - carry:.4f},{apex},4.5,-8.0,true\n")
            rows.append(f"session_5,shot_lw_12,{profile},20.0,25.0,5.0,15.0,2.0,1.0,true\n")
            rows.append(f"standard,drive_test_shot,{profile},{drive[0]},{drive[1]},{drive[1] - drive[0]:.4f},{drive[2]},5.9,-10.0,true\n")
        self.write(self.results, "".join(rows))

    def run_main(self, *extra):
        output = io.StringIO()
        with contextlib.redirect_stdout(output):
            code = compare_script.main(["--skip-run", "--library", str(self.library), "--results", str(self.results),
                                        "--history", str(self.history), *extra])
        return code, output.getvalue()

    def test_library_sessions_clubs_and_missing_references(self):
        shots = {(shot["session"], shot["shot"]): shot for shot in compare_script.load_library(self.library)}
        self.assertEqual(set(shots), {("session_5", "shot_driver_42"), ("session_5", "shot_lw_12"), ("standard", "drive_test_shot")})
        self.assertEqual(shots[("session_5", "shot_driver_42")]["club"], "driver")
        self.assertEqual(shots[("standard", "drive_test_shot")]["club"], "other")
        self.assertIsNone(shots[("session_5", "shot_lw_12")]["fs"]["carry"])
        self.assertAlmostEqual(shots[("standard", "drive_test_shot")]["fs"]["roll"], 12.0)

    def test_differences_are_engine_minus_flightscope(self):
        shots = compare_script.load_library(self.library)
        rows, missing = compare_script.compare(shots, compare_script.load_results(self.results), "current")
        self.assertEqual(missing, [])
        self.assertEqual([row["shot"] for row in rows], ["shot_driver_42", "drive_test_shot"])
        driver = rows[0]
        self.assertAlmostEqual(driver["diff_carry"], 3.0)
        self.assertAlmostEqual(driver["diff_total"], -6.0)
        self.assertAlmostEqual(driver["diff_roll"], -9.0)
        self.assertAlmostEqual(driver["diff_apex"], 1.0)
        self.assertEqual(driver["status"], "moderate")

    def test_missing_engine_result_is_reported(self):
        shots = compare_script.load_library(self.library)
        results = compare_script.load_results(self.results)
        del results[("standard", "drive_test_shot", "current")]
        rows, missing = compare_script.compare(shots, results, "current")
        self.assertEqual([shot["shot"] for shot in missing], ["drive_test_shot"])
        self.assertEqual(len(rows), 1)

    def test_grades_match_openfairway_thresholds(self):
        cases = [((3.0, 5.0), "pass"), ((-3.1, 0.0), "moderate"), ((0.0, 5.1), "moderate"),
                 ((7.1, 0.0), "severe"), ((0.0, -10.1), "severe"), ((7.0, 10.0), "moderate"), ((1.0, None), "pass")]
        for (carry, total), expected in cases:
            with self.subTest(carry=carry, total=total):
                self.assertEqual(compare_script.grade(carry, total), expected)

    def test_summary_statistics(self):
        rows = [{"diff_carry": value} for value in (2.0, -4.0, 6.0, -12.0)] + [{"diff_carry": None}]
        stats = compare_script.summarize(rows, "carry")
        self.assertEqual(stats["count"], 4)
        self.assertAlmostEqual(stats["bias"], -2.0)
        self.assertAlmostEqual(stats["avg"], 6.0)
        self.assertEqual(stats["worst"], -12.0)
        self.assertEqual(stats["within"], [25.0, 50.0, 75.0])
        self.assertIsNone(compare_script.summarize([{"diff_carry": None}], "carry"))

    def test_history_saves_only_changed_results(self):
        now = datetime(2026, 10, 15, 9, 30, 0)
        first, is_new, baseline = compare_script.record_history(self.history, "a\n", now)
        self.assertTrue(is_new)
        self.assertIsNone(baseline)
        same, is_new, baseline = compare_script.record_history(self.history, "a\n", now)
        self.assertEqual((same, is_new, baseline), (first, False, None))
        second, is_new, baseline = compare_script.record_history(self.history, "b\n", now)
        self.assertTrue(is_new)
        self.assertNotEqual(second, first)
        self.assertEqual(baseline, first)
        # Re-reporting unchanged results still compares with the last different run.
        _, is_new, baseline = compare_script.record_history(self.history, "b\n", now)
        self.assertFalse(is_new)
        self.assertEqual(baseline, first)

    def test_report_tables_and_change_since_previous_run(self):
        code, output = self.run_main()
        self.assertEqual(code, 0)
        self.assertIn("shot_driver_42", output)
        self.assertNotIn("shot_lw_12", output)
        self.assertIn("+3.0", output)
        self.assertIn("1 library shots have no FlightScope result", output)
        self.assertIn("No earlier run with different results", output)

        self.write_results(current=(194.9, 214.9, 39.1), historical=(190.9, 220.9, 37.1))
        code, output = self.run_main("--sort", "carry")
        self.assertEqual(code, 0)
        self.assertLess(output.index("drive_test_shot"), output.index("shot_driver_42"))
        self.assertIn("Change since run", output)
        self.assertIn("Carry (yd): avg|+/-| 3.70 -> 2.70 (-1.00)", output)

    def test_profiles_come_from_results(self):
        rows = [RESULTS_HEADER.rstrip("\n") + ",landing_angle_deg,landing_spin_rpm\n"]
        for profile in ("candidate-cd", "historical", "current"):
            rows.append(f"session_5,shot_driver_42,{profile},196.9,210.9,14.0,40.1,4.5,-8.0,true,38.5,1650.0\n")
            rows.append(f"standard,drive_test_shot,{profile},248.4,262.5,14.1,82.2,5.9,-10.0,true,40.0,1800.0\n")
        self.write(self.results, "".join(rows))
        results = compare_script.load_results(self.results)
        self.assertEqual(compare_script.profiles_in(results), ["current", "historical", "candidate-cd"])
        self.assertEqual(results[("session_5", "shot_driver_42", "current")]["landing_angle"], 38.5)
        self.assertEqual(results[("session_5", "shot_driver_42", "current")]["landing_spin"], 1650.0)
        code, output = self.run_main("--profile", "candidate-cd")
        self.assertEqual(code, 0)
        self.assertIn("Status (candidate-cd):", output)
        self.assertIn("Rollout by spin and landing angle (current profile):", output)

    def test_results_without_landing_columns_still_load(self):
        result = compare_script.load_results(self.results)[("session_5", "shot_driver_42", "current")]
        self.assertIsNone(result["landing_angle"])
        self.assertIsNone(result["landing_spin"])

    def test_rollout_breakdown_by_landing(self):
        def row(spin, angle, roll, fs_roll):
            return {"spin": spin, "landing_angle": angle, "roll": roll, "fs_roll": fs_roll,
                    "diff_roll": None if fs_roll is None else roll - fs_roll}
        rows = [row(2000.0, 30.0, 20.0, 25.0), row(3000.0, 20.0, 10.0, 15.0), row(6000.0, 50.0, 9.0, 2.0),
                row(4000.0, 35.0, 6.0, 5.0), row(7500.0, 45.0, 1.0, 0.5),
                row(8000.0, 40.0, 3.0, None), row(2000.0, None, 3.0, 4.0)]
        cells = compare_script.rollout_cells(rows)
        self.assertEqual(set(cells), {("Run", "<35"), ("Check", ">=45"), ("Release", "35-45"), ("Spinback", ">=45")})
        run = cells[("Run", "<35")]
        self.assertEqual(run["count"], 2)
        self.assertAlmostEqual(run["engine"], 15.0)
        self.assertAlmostEqual(run["fs"], 20.0)
        self.assertAlmostEqual(run["bias"], -5.0)
        self.assertAlmostEqual(cells[("Check", ">=45")]["bias"], 7.0)

    def test_filters_limit_rows_and_summary(self):
        code, output = self.run_main("--club", "driver")
        self.assertEqual(code, 0)
        self.assertIn("shot_driver_42", output)
        self.assertNotIn("drive_test_shot", output)
        self.assertIn("Summary over 1 shots", output)
        _, output = self.run_main("--club", "other")
        self.assertIn("drive_test_shot", output)
        self.assertNotIn("shot_driver_42", output)


if __name__ == "__main__":
    unittest.main()
