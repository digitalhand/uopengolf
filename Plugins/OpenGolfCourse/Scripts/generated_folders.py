# SPDX-License-Identifier: MIT
"""Remove superseded generated terrain folders.

The terrain builder writes each ground build to Build_<GUID> and each vegetation run's PCG graphs and
point data to Vegetation_<GUID>. Baking orphans a run's graphs, and the next build orphans the previous
ground, but nothing deleted them, so every rebuild added its folders to Git LFS for good. Call
remove_unreferenced after saving the level: the asset registry then matches the saved map, so a folder
it still uses is kept. Only folders named exactly as the builder names them are considered.

    py "<project>/Plugins/OpenGolfCourse/Scripts/generated_folders.py" --root /Game/UOpenGolf/Terrain/Generated [--root ...] [--apply]

Without --apply it only lists what it would remove. Save every open level first.
"""
import re

PREFIXES = ("Build_", "Vegetation_")


def is_run_folder(name, prefixes=PREFIXES):
    """True for a folder name the builder gives a run: a prefix and a 32-digit upper-case GUID."""
    return any(re.fullmatch(re.escape(prefix) + "[0-9A-F]{32}", name) for prefix in prefixes)


def run_folders(library, root, prefixes=PREFIXES):
    """Run folders that sit directly under root and contain assets."""
    folders = set()
    for path in library.list_assets(root, recursive=True, include_folder=False):
        relative = str(path)[len(root) + 1:]
        name, _, rest = relative.partition("/")
        if rest and is_run_folder(name, prefixes):
            folders.add(f"{root}/{name}")
    return folders


def referenced_from_outside(library, folder):
    """True if any package outside folder references an asset inside it."""
    for path in library.list_assets(folder, recursive=True, include_folder=False):
        package = str(path).split(".")[0]
        for referencer in library.find_package_referencers_for_asset(package, load_assets_to_confirm=True):
            if not str(referencer).startswith(folder + "/"):
                return True
    return False


def remove_unreferenced(library, roots, keep=(), prefixes=PREFIXES, apply=True, log=print):
    """Deletes unreferenced run folders under each root. Returns (removed, kept) folder lists.

    library is unreal.EditorAssetLibrary in the editor. With apply False nothing is deleted and
    removed lists what would go.
    """
    removed, kept = [], []
    for root in roots:
        for folder in sorted(run_folders(library, root, prefixes) - set(keep)):
            if referenced_from_outside(library, folder):
                kept.append(folder)
                log(f"Keeping {folder}: it is still referenced")
            elif not apply:
                removed.append(folder)
                log(f"Would remove unreferenced generated folder {folder}")
            elif library.delete_directory(folder):
                removed.append(folder)
                log(f"Removed unreferenced generated folder {folder}")
            else:
                kept.append(folder)
                log(f"Could not remove {folder}")
    return removed, kept


def main(argv):
    import argparse

    import unreal as ue

    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--root", action="append", required=True, help="Content folder holding Build_ and Vegetation_ folders")
    parser.add_argument("--apply", action="store_true", help="Delete; without it the folders are only listed")
    args = parser.parse_args(argv)
    if ue.get_editor_subsystem(ue.UnrealEditorSubsystem).get_game_world():
        raise RuntimeError("Stop Play before removing generated folders")
    dirty = ue.EditorLoadingAndSavingUtils.get_dirty_map_packages()
    if dirty:
        raise RuntimeError(f"Save the open levels first; unsaved: {[p.get_name() for p in dirty]}")
    removed, kept = remove_unreferenced(ue.EditorAssetLibrary, args.root, apply=args.apply, log=ue.log)
    ue.log(f"OPEN_GOLF_GENERATED_FOLDERS {'removed' if args.apply else 'would_remove'}={len(removed)} kept={len(kept)}")


if __name__ == "__main__":
    import sys

    main(sys.argv[1:])
