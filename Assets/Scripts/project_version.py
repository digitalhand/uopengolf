# SPDX-License-Identifier: MIT
"""Derive a package version from a Git ref and stamp it into Config/DefaultGame.ini."""
import argparse
from pathlib import Path
import re
import sys

DEFAULT_INI = Path(__file__).resolve().parent.parent / "Config" / "DefaultGame.ini"
SECTION = "[/Script/EngineSettings.GeneralProjectSettings]"
KEY = "ProjectVersion="
# MAJOR.MINOR.PATCH with an optional -prerelease; build metadata (+...) is not accepted.
VERSION = re.compile(r"(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)(-[0-9A-Za-z-]+(\.[0-9A-Za-z-]+)*)?")
COMMIT = re.compile(r"[0-9a-f]{7,40}")


def version_for_ref(ref_type, ref_name, sha, committed):
    """Return (version, is_release, prerelease) for a tag, or a dev version for a branch."""
    if ref_type == "tag":
        version = ref_name[1:] if ref_name.startswith("v") else ""
        if not VERSION.fullmatch(version):
            raise ValueError(f"Release tag {ref_name!r} must look like v1.2.3 or v1.2.3-rc.1.")
        return version, True, "-" in version
    if ref_type == "branch":
        if not COMMIT.fullmatch(sha):
            raise ValueError(f"Invalid commit SHA {sha!r}.")
        if not VERSION.fullmatch(committed):
            raise ValueError(f"Committed ProjectVersion {committed!r} is not a valid version.")
        # A committed prerelease such as 0.1.0-rc.1 gains another identifier instead of a second '-'.
        separator = "." if "-" in committed else "-"
        return f"{committed}{separator}dev.{sha[:7]}", False, True
    raise ValueError(f"Unsupported ref type {ref_type!r}; expected tag or branch.")


def _version_line(lines):
    """Return the index of the single ProjectVersion= line in the project settings section."""
    in_section = False
    found = []
    for index, line in enumerate(lines):
        stripped = line.strip()
        if stripped.startswith("["):
            in_section = stripped == SECTION
        elif in_section and stripped.startswith(KEY):
            found.append(index)
    if len(found) != 1:
        raise ValueError(f"Expected one {KEY} line in {SECTION}, found {len(found)}.")
    return found[0]


def read_project_version(text):
    """Return the ProjectVersion value from DefaultGame.ini text."""
    lines = text.splitlines()
    return lines[_version_line(lines)].strip()[len(KEY):]


def stamp_version(text, version):
    """Return DefaultGame.ini text with only ProjectVersion replaced, keeping line endings."""
    if not VERSION.fullmatch(version):
        raise ValueError(f"Invalid version {version!r}: expected MAJOR.MINOR.PATCH with an optional -prerelease.")
    lines = text.splitlines(keepends=True)
    index = _version_line(lines)
    ending = lines[index][len(lines[index].rstrip("\r\n")):]
    return "".join([*lines[:index], f"{KEY}{version}{ending}", *lines[index + 1:]])


def _read(path):
    with open(path, encoding="utf-8", newline="") as file:
        return file.read()


def main(argv):
    """Run the for-ref or stamp command; return the process exit status."""
    common = argparse.ArgumentParser(add_help=False)
    common.add_argument("--ini", type=Path, default=DEFAULT_INI, help="DefaultGame.ini to read or stamp")
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)
    for_ref = commands.add_parser("for-ref", parents=[common], help="print version outputs for a Git ref")
    for_ref.add_argument("ref_type", help="GITHUB_REF_TYPE: tag or branch")
    for_ref.add_argument("ref_name", help="GITHUB_REF_NAME, such as v1.2.3 or main")
    for_ref.add_argument("sha", help="GITHUB_SHA")
    stamp = commands.add_parser("stamp", parents=[common], help="write ProjectVersion into the ini")
    stamp.add_argument("version")
    arguments = parser.parse_args(argv)
    try:
        text = _read(arguments.ini)
        if arguments.command == "for-ref":
            committed = read_project_version(text)
            version, is_release, prerelease = version_for_ref(arguments.ref_type, arguments.ref_name, arguments.sha, committed)
            print(f"version={version}\nis_release={str(is_release).lower()}\nprerelease={str(prerelease).lower()}")
        else:
            stamped = stamp_version(text, arguments.version)
            with open(arguments.ini, "w", encoding="utf-8", newline="") as file:
                file.write(stamped)
            print(f"Set {KEY}{arguments.version} in {arguments.ini}")
    except (ValueError, OSError) as error:
        print(error, file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
