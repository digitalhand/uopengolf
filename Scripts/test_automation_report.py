# SPDX-License-Identifier: MIT
"""Hardware/engine-free checks for smoke report acceptance and rejection."""
import copy
import json
from pathlib import Path
import tempfile
import unittest

from check_automation_report import validate_report


SUCCESS = {
    "succeeded": 1,
    "succeededWithWarnings": 0,
    "failed": 0,
    "notRun": 0,
    "inProcess": 0,
    "tests": [{"fullTestPath": "UOpenGolf.Smoke.HelloWorld", "state": "Success"}],
}


class AutomationReportTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.path = Path(self.directory.name) / "index.json"

    def write(self, report):
        self.path.write_text(json.dumps(report), encoding="utf-8")

    def test_accepts_successful_required_test(self):
        self.write(SUCCESS)
        self.assertEqual(validate_report(self.path), 1)

    def test_accepts_success_with_warnings_counted_separately(self):
        report = copy.deepcopy(SUCCESS)
        report.update(succeeded=0, succeededWithWarnings=1)
        self.write(report)
        self.assertEqual(validate_report(self.path), 1)

    def test_rejects_zero_or_unrelated_tests(self):
        for tests in ([], [{"fullTestPath": "Engine.Unrelated", "state": "Success"}],
                      [{"fullTestPath": "UOpenGolf.Smoke.Other", "state": "Success"}]):
            with self.subTest(tests=tests):
                report = copy.deepcopy(SUCCESS)
                report["tests"] = tests
                self.write(report)
                with self.assertRaises(ValueError):
                    validate_report(self.path)

    def test_rejects_failed_or_incomplete_states(self):
        for state in ("Fail", "NotRun", "InProcess", "Skipped"):
            with self.subTest(state=state):
                report = copy.deepcopy(SUCCESS)
                report["tests"][0]["state"] = state
                self.write(report)
                with self.assertRaises(ValueError):
                    validate_report(self.path)

    def test_rejects_inconsistent_summary_counts(self):
        for changes in ({"failed": 1}, {"notRun": 1}, {"inProcess": 1}, {"succeeded": 0}):
            with self.subTest(changes=changes):
                report = copy.deepcopy(SUCCESS)
                report.update(changes)
                self.write(report)
                with self.assertRaises(ValueError):
                    validate_report(self.path)

    def test_rejects_missing_or_invalid_report(self):
        with self.assertRaises(OSError):
            validate_report(self.path)
        self.path.write_text('{"tests":', encoding="utf-8")
        with self.assertRaises(ValueError):
            validate_report(self.path)
        self.write({"tests": [{"state": "Success"}]})
        with self.assertRaises(KeyError):
            validate_report(self.path)


if __name__ == "__main__":
    unittest.main()
