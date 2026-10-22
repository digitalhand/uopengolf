# SPDX-License-Identifier: MIT
"""Fail closed when an Unreal automation report is missing, empty, or unsuccessful."""
import json
from pathlib import Path
import sys


def validate_report(path, prefix=None):
    """Validate a smoke report, or with prefix, a report of tests under that name prefix."""
    report = json.loads(Path(path).read_text(encoding="utf-8-sig"))
    tests = report["tests"]
    if not tests:
        raise ValueError("No tests were discovered.")
    if prefix is None:
        matched = [test for test in tests if test["fullTestPath"].startswith("UOpenGolf.Smoke.")]
        if not matched:
            raise ValueError("No UOpenGolf smoke tests were discovered.")
        if not any(test["fullTestPath"] == "UOpenGolf.Smoke.UOpenGolf" for test in matched):
            raise ValueError("The required UOpenGolf.Smoke.UOpenGolf test is missing.")
    else:
        matched = [test for test in tests if test["fullTestPath"] == prefix or test["fullTestPath"].startswith(prefix + ".")]
        if not matched:
            raise ValueError(f"No tests matching {prefix} were discovered.")
    for test in tests:
        if test["state"] != "Success":
            raise ValueError(f"{test['fullTestPath']}: {test['state']}")
    if report["failed"] != 0 or report["notRun"] != 0 or report["inProcess"] != 0:
        raise ValueError("Report contains failed, skipped, or incomplete tests.")
    succeeded = report["succeeded"] + report.get("succeededWithWarnings", 0)
    if succeeded != len(tests):
        raise ValueError("Report success count does not match its test results.")
    return len(matched)


if __name__ == "__main__":
    arguments = sys.argv[1:]
    test_prefix = None
    if len(arguments) == 3 and arguments[1] == "--prefix":
        test_prefix = arguments[2]
        arguments = arguments[:1]
    if len(arguments) != 1:
        sys.exit("Usage: check_automation_report.py PATH/TO/index.json [--prefix TEST.PREFIX]")
    try:
        count = validate_report(arguments[0], test_prefix)
    except (OSError, ValueError, KeyError, TypeError) as error:
        sys.exit(f"Automation report rejected: {error}")
    if test_prefix is None:
        print(f"Passed {count} uopengolf smoke test(s).")
    else:
        print(f"Passed {count} test(s) matching {test_prefix}.")
