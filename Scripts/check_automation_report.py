# SPDX-License-Identifier: MIT
"""Fail closed when an Unreal smoke report is missing, empty, or unsuccessful."""
import json
from pathlib import Path
import sys


def validate_report(path):
    report = json.loads(Path(path).read_text(encoding="utf-8-sig"))
    tests = report["tests"]
    if not tests:
        raise ValueError("No tests were discovered.")
    smoke_tests = [test for test in tests if test["fullTestPath"].startswith("UOpenGolf.Smoke.")]
    if not smoke_tests:
        raise ValueError("No UOpenGolf smoke tests were discovered.")
    if not any(test["fullTestPath"] == "UOpenGolf.Smoke.HelloWorld" for test in smoke_tests):
        raise ValueError("The required UOpenGolf.Smoke.HelloWorld test is missing.")
    for test in tests:
        if test["state"] != "Success":
            raise ValueError(f"{test['fullTestPath']}: {test['state']}")
    if report["failed"] != 0 or report["notRun"] != 0 or report["inProcess"] != 0:
        raise ValueError("Report contains failed, skipped, or incomplete tests.")
    succeeded = report["succeeded"] + report.get("succeededWithWarnings", 0)
    if succeeded != len(tests):
        raise ValueError("Report success count does not match its test results.")
    return len(smoke_tests)


if __name__ == "__main__":
    if len(sys.argv) != 2:
        sys.exit("Usage: check_automation_report.py PATH/TO/index.json")
    try:
        count = validate_report(sys.argv[1])
    except (OSError, ValueError, KeyError, TypeError) as error:
        sys.exit(f"Smoke report rejected: {error}")
    print(f"Passed {count} uopengolf smoke test(s).")
