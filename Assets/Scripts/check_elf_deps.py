# SPDX-License-Identifier: MIT
"""Fail when any ELF file under a folder has a library dependency this system can't resolve.

    python3 -B Assets/Scripts/check_elf_deps.py /opt/uopengolf

The install checks run it inside a distribution's container, so the result is that distribution's, not the
build machine's.
"""
from pathlib import Path
import subprocess
import sys


def is_elf(path: Path) -> bool:
    with path.open("rb") as stream:
        return stream.read(4) == b"\x7fELF"


def unresolved(root: Path) -> list[str]:
    problems = []
    for path in sorted(root.rglob("*")):
        if path.is_file() and not path.is_symlink() and is_elf(path):
            result = subprocess.run(["ldd", str(path)], text=True, capture_output=True)
            if result.returncode or "not found" in result.stdout:
                problems.append(f"{path}: {result.stdout} {result.stderr}")
    return problems


def main(argv: list[str]) -> int:
    if len(argv) != 1:
        print(__doc__, file=sys.stderr)
        return 2
    problems = unresolved(Path(argv[0]))
    for problem in problems:
        print(problem, file=sys.stderr)
    if not problems:
        print(f"All ELF dependencies under {argv[0]} resolve.")
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
