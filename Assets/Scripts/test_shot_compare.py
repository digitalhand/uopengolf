# SPDX-License-Identifier: MIT
"""Engine-free checks for the shot comparison script."""
import contextlib
from datetime import datetime
import io
from pathlib import Path
import tempfile
import unittest

import Assets.Scripts.shot_compare as compare_script


LIBRARY_HEADER = ("shot_name,reported_club,speed_mph,vla_deg,hla_deg,total_spin_rpm,spin_axis_deg,backspin_rpm,sidespin_rpm,"
                  "carry_yd,total_yd,rollout_yd,apex_ft,landing_angle_deg,offline_yd\n")
RESULTS_HEADER = ("session,shot_name,profile,carry_yd,total_yd,rollout_yd,apex_ft,hang_s,lateral_yd,at_rest,"
                  "landing_angle_deg,landing_spin_rpm\n")


class ShotCompareTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)
        self.library = self.root / "ShotLibrary"
        self.results = self.root / "results.csv"
        self.history = self.root / "history"
        self.write(self.library / "square_20261113" / "shots.csv", LIBRARY_HEADER
                   + "7iron_01,7 Iron,118.00,17.80,-0.20,5436,1.00,5435,94,172.20,176.20,4.00,95.80,45.70,1.00\n"
                   # A ball hit along the ground carries 0 and is still compared.
                   + "6iron_15,6 Iron,70.80,-1.80,6.90,1273,0.00,1273,0,0.00,90.10,90.10,0.00,0.00,0.00\n"
                   # A shot with no reference total is replayed but not compared.
                   + "6iron_99,6 Iron,60.00,10.00,0.00,3000,0.00,3000,0,,,,,,\n")
        self.write_results(current=(175.2, 178.2, 96.8), ground=(0.0, 95.1))

    @staticmethod
    def write(path, text):
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text, encoding="utf-8")

    def write_results(self, current, ground, historical=(170.2, 180.2, 94.8)):
        rows = [RESULTS_HEADER]
        for profile, (carry, total, apex) in (("current", current), ("historical", historical)):
            rows.append(f"square_20261113,7iron_01,{profile},{carry},{total},{total - carry:.4f},{apex},5.0,1.5,true,44.7,4800.0\n")
            rows.append(f"square_20261113,6iron_15,{profile},{ground[0]},{ground[1]},{ground[1] - ground[0]:.4f},0.2,0.0,8.0,true,0.4,1200.0\n")
            rows.append(f"square_20261113,6iron_99,{profile},50.0,60.0,10.0,20.0,3.0,0.0,true,30.0,2500.0\n")
        self.write(self.results, "".join(rows))

    def run_main(self, *extra):
        output = io.StringIO()
        with contextlib.redirect_stdout(output):
            code = compare_script.main(["--skip-run", "--library", str(self.library), "--results", str(self.results),
                                        "--history", str(self.history), *extra])
        return code, output.getvalue()

    def test_library_sessions_and_references(self):
        shots = {shot["shot"]: shot for shot in compare_script.load_library(self.library)}
        self.assertEqual(set(shots), {"7iron_01", "6iron_15", "6iron_99"})
        self.assertEqual(shots["7iron_01"]["session"], "square_20261113")
        self.assertAlmostEqual(shots["7iron_01"]["speed"], 118.0)
        self.assertAlmostEqual(shots["7iron_01"]["ref"]["roll"], 4.0)
        self.assertAlmostEqual(shots["7iron_01"]["ref"]["landing"], 45.7)
        self.assertEqual(shots["6iron_15"]["ref"]["carry"], 0.0)
        self.assertAlmostEqual(shots["6iron_15"]["ref"]["roll"], 90.1)
        self.assertIsNone(shots["6iron_99"]["ref"]["total"])

    def test_reported_club_is_never_read(self):
        shots = compare_script.load_library(self.library)
        self.assertTrue(all("club" not in key for shot in shots for key in shot))
        _, output = self.run_main()
        self.assertNotIn("7 Iron", output)
        self.assertNotIn("6 Iron", output)
        # Relabelling every shot changes nothing in the report.
        relabelled = self.library / "square_20261113" / "shots.csv"
        relabelled.write_text(relabelled.read_text(encoding="utf-8").replace("7 Iron", "Driver").replace("6 Iron", "Putter"), encoding="utf-8")
        _, again = self.run_main()
        self.assertEqual(output.split("Engine results:")[0], again.split("Engine results:")[0])

    def test_differences_are_engine_minus_reference(self):
        shots = compare_script.load_library(self.library)
        rows, missing = compare_script.compare(shots, compare_script.load_results(self.results), "current")
        self.assertEqual(missing, [])
        self.assertEqual([row["shot"] for row in rows], ["7iron_01", "6iron_15"])
        iron, ground = rows
        self.assertAlmostEqual(iron["diff_carry"], 3.0)
        self.assertAlmostEqual(iron["diff_total"], 2.0)
        self.assertAlmostEqual(iron["diff_roll"], -1.0)
        self.assertAlmostEqual(iron["diff_apex"], 1.0)
        self.assertAlmostEqual(iron["diff_landing"], -1.0)
        self.assertEqual(iron["status"], "pass")
        self.assertAlmostEqual(ground["diff_roll"], 5.0)
        self.assertEqual(ground["status"], "pass")
        self.assertEqual(ground["class"], "ground (<2.5 deg)")
        self.assertEqual(iron["class"], "mid (9-20 deg, >=4000 rpm)")

    def test_missing_engine_result_is_reported(self):
        shots = compare_script.load_library(self.library)
        results = compare_script.load_results(self.results)
        del results[("square_20261113", "6iron_15", "current")]
        rows, missing = compare_script.compare(shots, results, "current")
        self.assertEqual([shot["shot"] for shot in missing], ["6iron_15"])
        self.assertEqual(len(rows), 1)

    def test_grades(self):
        cases = [((3.0, 5.0), "pass"), ((-3.1, 0.0), "moderate"), ((0.0, 5.1), "moderate"),
                 ((7.1, 0.0), "severe"), ((0.0, -10.1), "severe"), ((7.0, 10.0), "moderate"), ((1.0, None), "pass")]
        for (carry, total), expected in cases:
            with self.subTest(carry=carry, total=total):
                self.assertEqual(compare_script.grade(carry, total), expected)

    def test_launch_classes(self):
        self.assertEqual(compare_script.launch_class(-1.0, 1500.0), "ground (<2.5 deg)")
        self.assertEqual(compare_script.launch_class(5.0, 2500.0), "low (2.5-9 deg)")
        self.assertEqual(compare_script.launch_class(12.0, 3999.0), "mid (9-20 deg, <4000 rpm)")
        self.assertEqual(compare_script.launch_class(25.0, 8000.0), "high (>=20 deg)")
        labels = ["high (>=20 deg)", "mid (9-20 deg, >=4000 rpm)", "ground (<2.5 deg)", "mid (9-20 deg, <4000 rpm)", "low (2.5-9 deg)"]
        self.assertEqual(sorted(labels, key=compare_script.class_order),
                         ["ground (<2.5 deg)", "low (2.5-9 deg)", "mid (9-20 deg, <4000 rpm)", "mid (9-20 deg, >=4000 rpm)", "high (>=20 deg)"])

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
        now = datetime(2026, 11, 16, 9, 30, 0)
        first, is_new, baseline = compare_script.record_history(self.history, "a\n", now)
        self.assertTrue(is_new)
        self.assertIsNone(baseline)
        same, is_new, baseline = compare_script.record_history(self.history, "a\n", now)
        self.assertEqual((same, is_new, baseline), (first, False, None))
        second, is_new, baseline = compare_script.record_history(self.history, "b\n", now)
        self.assertTrue(is_new)
        self.assertNotEqual(second, first)
        self.assertEqual(baseline, first)
        _, is_new, baseline = compare_script.record_history(self.history, "b\n", now)
        self.assertFalse(is_new)
        self.assertEqual(baseline, first)

    def test_report_tables_and_change_since_previous_run(self):
        code, output = self.run_main()
        self.assertEqual(code, 0)
        self.assertIn("7iron_01", output)
        self.assertIn("6iron_15", output)
        self.assertNotIn("6iron_99", output.split("Summary over")[0])
        self.assertIn("1 library shots have no reference total", output)
        self.assertIn("Roll by launch class:", output)
        self.assertIn("ground (<2.5 deg)", output)
        self.assertIn("No earlier run with different results", output)

        self.write_results(current=(174.2, 177.2, 96.8), ground=(0.0, 92.1))
        code, output = self.run_main("--sort", "roll")
        self.assertEqual(code, 0)
        self.assertLess(output.index("6iron_15"), output.index("7iron_01"))
        self.assertIn("Change since run", output)
        self.assertIn("Carry (yd): avg|+/-| 1.50 -> 1.00 (-0.50)", output)

    def test_a_baseline_without_these_shots_says_so(self):
        old = self.history / "20261001-120000.csv"
        self.write(old, ",".join(compare_script.SNAPSHOT_FIELDS) + "\nsession_5,shot_driver_42,current,pass" + "," * 15 + "\n")
        code, output = self.run_main()
        self.assertEqual(code, 0)
        self.assertIn("has none of these shots", output)

    def test_errors_by_ball_speed(self):
        code, output = self.run_main()
        self.assertEqual(code, 0)
        table = output.split("By ball speed (current profile):")[1].split("\n\n")[0]
        # 70.8 mph (ground) and 118.0 mph; the 60 mph shot has no reference total.
        self.assertIn("60-75 mph", table)
        self.assertIn(">=105 mph", table)
        self.assertNotIn("<60 mph", table)
        self.assertEqual(compare_script.bin_label(60.0, compare_script.SPEED_BANDS), "60-75 mph")
        self.assertEqual(compare_script.bin_label(59.9, compare_script.SPEED_BANDS), "<60 mph")

    def test_club_filter_is_gone(self):
        with self.assertRaises(SystemExit), contextlib.redirect_stderr(io.StringIO()):
            compare_script.parse_args(["--skip-run", "--club", "7 Iron"])

    def test_accuracy_gates_use_launch_values(self):
        shots = compare_script.load_library(self.library)
        results = compare_script.load_results(self.results)
        original = compare_script.validate_accuracy(shots, results, results)
        self.assertTrue(any("25%" in error for error in original))

    def test_accuracy_rejects_missing_nonfinite_and_nonresting_results(self):
        shots = compare_script.load_library(self.library)
        baseline = compare_script.load_results(self.results)
        results = compare_script.load_results(self.results)
        key = ("square_20261113", "6iron_99", "current")  # No reference, but still must terminate.
        results[key]["at_rest"] = False
        self.assertTrue(any("Nonfinite or non-resting" in error for error in compare_script.validate_accuracy(shots, results, baseline)))
        results[key]["at_rest"] = True
        results[key]["total"] = float("nan")
        self.assertTrue(compare_script.validate_accuracy(shots, results, baseline))
        del results[key]
        self.assertTrue(any("Missing result" in error for error in compare_script.validate_accuracy(shots, results, baseline)))

    def test_check_requires_verified_provenance(self):
        self.assertTrue(compare_script.validate_provenance(self.results, self.library))

    def test_duplicate_results_are_rejected(self):
        with self.results.open("a", encoding="utf-8") as handle:
            handle.write(self.results.read_text(encoding="utf-8").splitlines()[1] + "\n")
        with self.assertRaisesRegex(ValueError, "Duplicate result"):
            compare_script.load_results(self.results)


if __name__ == "__main__":
    unittest.main()
