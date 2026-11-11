# SPDX-License-Identifier: MIT
"""Fail closed unless a game start log shows the startup map loaded without a crash."""
from pathlib import Path
import re
import sys

STARTUP_MAP = "/Game/UOpenGolf/Maps/UOpenGolf"
CRASH_MARKERS = ("Fatal error", "Assertion failed", "Unhandled Exception", "appError called")


def validate_start_log(path, map_path=STARTUP_MAP):
    """Raise ValueError unless the log at path loaded map_path and shows no crash."""
    log = Path(path)
    if not log.is_file():
        raise ValueError(f"Start log not found: {log}")
    text = log.read_text(encoding="utf-8", errors="replace")
    if not text.strip():
        raise ValueError(f"Start log is empty: {log}")
    # A crash after the map loaded still fails, so check the crash markers first.
    for marker in CRASH_MARKERS:
        if marker in text:
            raise ValueError(f"Start log contains '{marker}': {log}")
    if not re.search(r"Took [0-9.]+ seconds to LoadMap\(" + re.escape(map_path) + r"\)", text):
        raise ValueError(f"Start log never reached LoadMap({map_path}): {log}")


if __name__ == "__main__":
    if len(sys.argv) != 2:
        print("Usage: check_package_start.py GAME_LOG", file=sys.stderr)
        sys.exit(2)
    try:
        validate_start_log(sys.argv[1])
    except ValueError as error:
        print(error, file=sys.stderr)
        sys.exit(1)
    print(f"Package start check passed: {sys.argv[1]}")
