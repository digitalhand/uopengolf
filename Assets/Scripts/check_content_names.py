# SPDX-License-Identifier: MIT
"""Check asset and folder names in every content mount against Docs/ContentNaming.md.

    python3 Assets/Scripts/check_content_names.py [--project <dir>]

Scans Content/ and Plugins/*/Content/ on disk without the editor. An asset name needs a known type prefix,
PascalCase segments, two-digit variants and, for a texture, a role suffix (asset_naming.problems). A folder
needs a PascalCase name, or the exact form a generator gives a build (Build_<GUID>, Tile_<x>_<y>_<GUID>).
The class behind each prefix is checked in the editor by the UOpenGolf.Content.Naming automation test.

Exceptions are listed, with a reason each, in content_name_exceptions.txt beside this script. Exit status 1
lists every other problem.
"""
from pathlib import Path
import argparse
import fnmatch
import re
import sys

PROJECT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(PROJECT / "Plugins/OpenGolfCourse/Scripts"))
import asset_naming  # noqa: E402

EXCEPTIONS = Path(__file__).with_name("content_name_exceptions.txt")
PACKAGE_SUFFIXES = (".uasset", ".umap")
GUID = "[0-9A-F]{32}"
# Folders a generator creates for one build. Anything inside is named by the same generator.
GENERATED_FOLDER = re.compile(rf"(?:Build|Vegetation|Water)_{GUID}|Tile_-?\d+_-?\d+_{GUID}")
FOLDER = re.compile(r"[A-Z][A-Za-z0-9]*")
# Folders the engine owns: never contain project assets, or have names fixed by the engine.
ENGINE_FOLDERS = {"Collections", "Developers", "Splash", "__ExternalActors__", "__ExternalObjects__"}
BUILT_DATA = "_BuiltData"


def content_roots(project):
    """(mount, folder) for /Game and each plugin with content."""
    roots = [("/Game", project / "Content")]
    for plugin in sorted((project / "Plugins").glob("*/Content")):
        roots.append(("/" + plugin.parent.name, plugin))
    return [(mount, folder) for mount, folder in roots if folder.is_dir()]


def read_exceptions(path=EXCEPTIONS):
    """Content-path patterns (fnmatch) that are allowed to break the rules; '#' starts a reason or comment."""
    if not path.is_file():
        return []
    patterns = []
    for line in path.read_text().splitlines():
        pattern = line.split("#", 1)[0].strip()
        if pattern:
            patterns.append(pattern)
    return patterns


def asset_problems(name):
    """Problems for an asset file's name (without extension)."""
    if name.endswith(BUILT_DATA):
        # The engine names a map's lighting data after the map.
        name = name[: -len(BUILT_DATA)]
        if not name.startswith("L_"):
            return ["built data belongs to a map without the L_ prefix"]
    return asset_naming.problems(name)


def folder_problems(name):
    if name in ENGINE_FOLDERS or GENERATED_FOLDER.fullmatch(name):
        return []
    found = []
    if not FOLDER.fullmatch(name):
        found.append("folder is not PascalCase")
    lower = name.lower()
    found += [f"folder contains '{word}'" for word in asset_naming.FORBIDDEN_WORDS if word in lower]
    return found


def scan(project):
    """{content path: [problems]} for every misnamed asset and folder."""
    found = {}
    for mount, root in content_roots(project):
        for path in sorted(root.rglob("*")):
            relative = path.relative_to(root)
            if any(part in ENGINE_FOLDERS for part in relative.parts[:-1]):
                continue
            content_path = f"{mount}/{relative.with_suffix('').as_posix()}"
            if path.is_dir():
                problems = folder_problems(path.name)
            elif path.suffix in PACKAGE_SUFFIXES:
                problems = asset_problems(path.stem)
            else:
                continue
            if problems:
                found[content_path] = problems
    return found


def unexcepted(found, patterns):
    return {path: problems for path, problems in found.items()
            if not any(fnmatch.fnmatchcase(path, pattern) for pattern in patterns)}


def unused_exceptions(found, patterns):
    """Patterns that no longer match a problem, so the list can shrink."""
    return [p for p in patterns if not any(fnmatch.fnmatchcase(path, p) for path in found)]


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--project", type=Path, default=PROJECT)
    args = parser.parse_args(argv)
    found = scan(args.project)
    patterns = read_exceptions()
    remaining = unexcepted(found, patterns)
    for path, problems in sorted(remaining.items()):
        print(f"{path}: {'; '.join(problems)}")
    for pattern in unused_exceptions(found, patterns):
        print(f"Unused exception (remove it): {pattern}")
    print(f"CONTENT_NAMES problems={len(remaining)} excepted={len(found) - len(remaining)}")
    return 1 if remaining else 0


if __name__ == "__main__":
    sys.exit(main())
