# SPDX-License-Identifier: MIT
"""Engine-free checks for the carry audit and its before/after report."""
import contextlib
import io
from pathlib import Path
import tempfile
import unittest

import Assets.Scripts.carry_audit as carry_audit

LIBRARY_HEADER = "shot_name,speed_mph,vla_deg,hla_deg,total_spin_rpm,spin_axis_deg,backspin_rpm,sidespin_rpm,carry_yd,total_yd,apex_ft,landing_angle_deg\n"
RESULTS_HEADER = "session,shot_name,profile,carry_yd,total_yd,rollout_yd,apex_ft,hang_s,lateral_yd,at_rest,landing_angle_deg,landing_spin_rpm\n"


class CarryAuditTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        root = Path(self.directory.name)
        self.library = root / "ShotLibrary"
        (self.library / "ref_session").mkdir(parents=True)
        (self.library / "ref_session" / "shots.csv").write_text(
            LIBRARY_HEADER + "a,150,12,0,2400,0,2400,0,250.0,270.0,90.0,40.0\nb,100,20,0,6000,0,6000,0,140.0,145.0,80.0,48.0\n", encoding="utf-8")
        self.before = root / "before.csv"
        self.after = root / "after.csv"
        self.before.write_text(RESULTS_HEADER + "ref_session,a,current,252.0,272.0,20.0,91.0,6.0,0.0,true,41.0,2000\n"
                               "ref_session,b,current,139.0,144.0,5.0,79.0,5.0,0.0,true,47.0,5000\n", encoding="utf-8")
        self.after.write_text(RESULTS_HEADER + "ref_session,a,current,251.0,271.5,20.5,90.5,6.0,0.0,true,40.5,2000\n"
                              "ref_session,b,current,139.0,144.0,5.0,79.0,5.0,0.0,true,47.0,5000\n", encoding="utf-8")

    def run_main(self, *extra):
        output = io.StringIO()
        with contextlib.redirect_stdout(output):
            code = carry_audit.main(["--library", str(self.library), "--results", str(self.after), *extra])
        return code, output.getvalue()

    def test_errors_are_engine_minus_reference(self):
        _, output = self.run_main()
        # Carry errors +1.0 and -1.0: bias 0, mean absolute error 1.
        self.assertIn("carry yd  +0.00 /  1.00", output)

    def test_before_and_after(self):
        _, output = self.run_main("--before", str(self.before))
        self.assertIn("before", output)
        self.assertIn("now", output)
        self.assertIn("carry    -1.0000 (ref_session/a)", output)

    def test_markdown_report(self):
        report = Path(self.directory.name) / "report.md"
        self.run_main("--before", str(self.before), "--markdown", str(report))
        text = report.read_text(encoding="utf-8")
        self.assertIn("| ref_session | 1.50 | 1.00 | +0.00 | 1.00 |", text)
        self.assertIn("| ref_session / a | -1.000 | -0.500 | +0.500 | -0.500 | -0.500 |", text)

    def test_markdown_needs_before(self):
        with self.assertRaises(SystemExit), contextlib.redirect_stderr(io.StringIO()):
            self.run_main("--markdown", "x.md")


if __name__ == "__main__":
    unittest.main()
