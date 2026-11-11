# SPDX-License-Identifier: MIT
"""Delete listed assets that nothing uses, after backing them up.

    py "<project>/Scripts/prune_unreferenced_assets.py" --manifest <file.json> [--apply]

The manifest is a JSON list of content paths: an asset (/Game/Folder/Asset) or a whole folder
(/Game/Folder/, ending in a slash). A listed asset is deleted only if:
- every package that references it is itself being deleted (asset registry, loading to confirm), and
- no C++ or Python source in the project names it, by full path or by its folder and name in one file.

Without --apply it only reports each decision. With --apply it first copies every file to
Saved/AssetBackups/<UTC stamp>/, keeping its path below the project, then deletes the asset.
Run in the editor with Play stopped and every level saved. Completion: OPEN_GOLF_PRUNE_COMPLETE.
"""
from pathlib import Path
import re

SOURCE_DIRS = ("Source", "Plugins/*/Source", "Scripts", "Plugins/*/Scripts")
SOURCE_SUFFIXES = (".cpp", ".h", ".py", ".cs")
PACKAGE_SUFFIXES = (".uasset", ".umap")
# This tool and its tests mention asset paths only as examples.
OWN_FILES = ("prune_unreferenced_assets.py", "test_prune_unreferenced_assets.py")


def expand(library, entries):
    """Packages for manifest entries: a folder (ending in /) expands to every asset under it."""
    packages = set()
    for entry in entries:
        paths = library.list_assets(entry.rstrip("/"), recursive=True, include_folder=False) if entry.endswith("/") else [entry]
        packages.update(str(path).split(".")[0] for path in paths)
    return packages


def named_in_sources(packages, sources):
    """{package: source} for packages a source names, by full path or by folder and whole-word name in one file."""
    named = {}
    for package in sorted(packages):
        folder, name = package.rsplit("/", 1)
        word = re.compile(r"(?<![A-Za-z0-9_])" + re.escape(name) + r"(?![A-Za-z0-9_])")
        for source, text in sources.items():
            if package + "." in text or f"{package}'" in text or f'{package}"' in text or (folder in text and word.search(text)):
                named[package] = source
                break
    return named


def decide(packages, referencers, named):
    """(deletable in sorted order, {kept package: reason}). Anything a kept asset uses is kept too."""
    kept = {package: f"named in {source}" for package, source in named.items() if package in packages}
    for package in packages:
        outside = [r for r in referencers.get(package, []) if r not in packages]
        if package not in kept and outside:
            kept[package] = f"referenced by {outside[0]}"
    changed = True
    while changed:
        changed = False
        for package in sorted(packages - set(kept)):
            user = next((r for r in referencers.get(package, []) if r != package and r in kept), None)
            if user:
                kept[package] = f"used by kept {user}"
                changed = True
    return sorted(packages - set(kept)), kept


def package_file(package, mounts):
    """The file path, without extension, of a package under mounts {mount name: content folder}."""
    mount, _, relative = package.strip("/").partition("/")
    if mount not in mounts:
        raise ValueError(f"Unknown content mount for {package}")
    return mounts[mount] / relative


def content_mounts(project):
    mounts = {"Game": project / "Content"}
    for descriptor in (project / "Plugins").rglob("*.uplugin"):
        if (descriptor.parent / "Content").is_dir():
            mounts[descriptor.stem] = descriptor.parent / "Content"
    return mounts


def read_sources(project):
    sources = {}
    for pattern in SOURCE_DIRS:
        for folder in project.glob(pattern):
            for path in folder.rglob("*"):
                if path.suffix in SOURCE_SUFFIXES and "__pycache__" not in path.parts and path.name not in OWN_FILES:
                    sources[str(path.relative_to(project))] = path.read_text(encoding="utf-8", errors="replace")
    return sources


def main(argv):
    import argparse
    from datetime import datetime, timezone
    import json
    import shutil

    import unreal as ue

    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--manifest", required=True)
    parser.add_argument("--apply", action="store_true")
    args = parser.parse_args(argv)
    if ue.get_editor_subsystem(ue.UnrealEditorSubsystem).get_game_world():
        raise RuntimeError("Stop Play before pruning assets")
    dirty = ue.EditorLoadingAndSavingUtils.get_dirty_map_packages() + ue.EditorLoadingAndSavingUtils.get_dirty_content_packages()
    if dirty:
        raise RuntimeError(f"Save every changed package first; unsaved: {[p.get_name() for p in dirty]}")

    library = ue.EditorAssetLibrary
    project = Path(ue.Paths.project_dir()).resolve()
    packages = expand(library, json.loads(Path(args.manifest).read_text(encoding="utf-8")))
    referencers = {p: [str(r) for r in library.find_package_referencers_for_asset(p, load_assets_to_confirm=True)] for p in sorted(packages)}
    deletable, kept = decide(packages, referencers, named_in_sources(packages, read_sources(project)))
    for package, reason in sorted(kept.items()):
        ue.log(f"Keeping {package}: {reason}")
    if not args.apply:
        for package in deletable:
            ue.log(f"Would delete {package}")
        ue.log(f"OPEN_GOLF_PRUNE_COMPLETE would_delete={len(deletable)} kept={len(kept)}")
        return

    mounts = content_mounts(project)
    backup = project / "Saved/AssetBackups" / datetime.now(timezone.utc).strftime("%Y%m%d-%H%M%S")
    for package in deletable:
        base = package_file(package, mounts)
        files = [base.with_suffix(suffix) for suffix in PACKAGE_SUFFIXES if base.with_suffix(suffix).is_file()]
        if not files:
            raise RuntimeError(f"No file found for {package}; nothing was deleted after this point")
        for file in files:
            target = backup / file.relative_to(project)
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(file, target)
    deleted = [package for package in deletable if library.delete_asset(package)]
    for package in sorted(set(deletable) - set(deleted)):
        ue.log_warning(f"Could not delete {package}")
    ue.log(f"OPEN_GOLF_PRUNE_COMPLETE deleted={len(deleted)} failed={len(deletable) - len(deleted)} kept={len(kept)} backup={backup}")


if __name__ == "__main__":
    import sys

    main(sys.argv[1:])
