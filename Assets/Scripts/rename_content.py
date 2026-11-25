# SPDX-License-Identifier: MIT
"""Apply the content rename (content_renames.py) in the editor, then relabel the range map's actors.

    py "<project>/Assets/Scripts/rename_content.py" [--apply]

Headless, with no other editor running:
    UnrealEditor <project>/UOpenGolf.uproject -nullrhi -unattended -nosound -DisablePlugins=ModelContextProtocol \\
      "-ExecutePythonScript=<project>/Assets/Scripts/rename_content.py --apply --quit"

Without --apply it logs every planned move and label. With --apply it first copies every content folder to
Saved/AssetBackups/<UTC stamp>-content-rename/, then:
1. renames and moves the assets in one batch, which updates every loaded and unloaded referencer;
2. fixes up and deletes the redirectors the rename left, so no old path remains, and remaps the tree meshes'
   OpenGolf.WoodProxy metadata, which stores a path;
3. deletes the unused club-face textures and unreferenced generated vegetation runs;
4. relabels the range map's actors and saves it.
Completion: OPEN_GOLF_CONTENT_RENAME_COMPLETE.
"""
from datetime import datetime, timezone
from pathlib import Path
import argparse
import shutil
import sys

import unreal as ue

PROJECT = Path(ue.Paths.project_dir()).resolve()
sys.path.insert(0, str(PROJECT))
sys.path.insert(0, str(PROJECT / "Plugins/OpenGolfCourse/Scripts"))
from Assets.Scripts import content_renames as renames  # noqa: E402
import generated_folders  # noqa: E402

MOUNTS = {"/Game": PROJECT / "Content", "/OpenGolfCourse": PROJECT / "Plugins/OpenGolfCourse/Content",
          "/OpenGolfSavanna": PROJECT / "Plugins/OpenGolfSavanna/Content", "/HunterRanch": PROJECT / "Plugins/HunterRanch/Content",
          "/OpenGolfEnvironment": PROJECT / "Plugins/OpenGolfEnvironment/Content"}
RANGE_MAP = "/Game/UOpenGolf/Maps/L_SavannaRange"
VEGETATION_RUNS = "/Game/UOpenGolf/Terrain/Savanna/CutTurf"
REDIRECTOR = ue.TopLevelAssetPath("/Script/CoreUObject", "ObjectRedirector")
WOOD_PROXY = "OpenGolf.WoodProxy"


def registry():
    return ue.AssetRegistryHelpers.get_asset_registry()


def packages():
    """Every package under the project's content mounts."""
    found = set()
    for mount in MOUNTS:
        for data in registry().get_assets_by_path(mount, recursive=True):
            if not data.is_redirector():
                found.add(str(data.package_name))
    return found


def backup():
    target = PROJECT / "Saved/AssetBackups" / (datetime.now(timezone.utc).strftime("%Y%m%d-%H%M%S") + "-content-rename")
    for mount, folder in MOUNTS.items():
        if folder.is_dir():
            shutil.copytree(folder, target / folder.relative_to(PROJECT))
    ue.log(f"Content backed up to {target}")
    return target


def rename(moves):
    """Renames in one batch so references between moved assets are rewritten once. Maps go first, on their own."""
    tools = ue.AssetToolsHelpers.get_asset_tools()
    worlds = {old: new for old, new in moves.items() if not old.endswith("_BuiltData") and is_world(old)}
    for batch in (worlds, {o: n for o, n in moves.items() if o not in worlds}):
        data = []
        for old, new in sorted(batch.items()):
            if not ue.EditorAssetLibrary.does_asset_exist(old):
                ue.log_warning(f"Skipping {old}: it no longer exists (renamed with its map?)")
                continue
            asset = ue.load_asset(old)
            if not asset:
                raise RuntimeError(f"Could not load {old}")
            folder, name = new.rsplit("/", 1)
            data.append(ue.AssetRenameData(asset=asset, new_package_path=folder, new_name=name))
        if data and not tools.rename_assets(data):
            raise RuntimeError("The rename failed part-way; restore the backup")
        ue.log(f"Renamed {len(data)} assets")


def is_world(package):
    data = registry().get_assets_by_package_name(package)
    return any(str(d.asset_class_path.asset_name) == "World" for d in data)


def fix_redirectors():
    """Points referencers at the renamed assets and deletes the redirectors (UOpenGolfContentTools, C++: the engine
    does not expose its fix-up to Python)."""
    found, fixed = ue.OpenGolfContentTools.fix_up_redirectors(list(MOUNTS))
    left = [str(d.package_name) for d in registry().get_assets_by_class(REDIRECTOR)
            if "/" + str(d.package_name).split("/")[1] in MOUNTS]
    ue.log(f"Fixed up {fixed} of {found} redirectors; {len(left)} left")
    if left:
        raise RuntimeError(f"Redirectors are still referenced: {left[:10]}")


def remap_metadata():
    """Metadata that stores a package path: a tree mesh's OpenGolf.WoodProxy names its collision mesh."""
    library = ue.EditorAssetLibrary
    for mount in MOUNTS:
        for path in library.list_assets(mount, recursive=True, include_folder=False):
            data = library.find_asset_data(path)
            if str(data.asset_class_path.asset_name) != "StaticMesh":
                continue
            mesh = library.load_asset(path)
            proxy = library.get_metadata_tag(mesh, WOOD_PROXY)
            target = renames.move(proxy) if proxy else proxy
            if proxy and target != proxy:
                library.set_metadata_tag(mesh, WOOD_PROXY, target)
                library.save_loaded_asset(mesh)
                ue.log(f"Remapped {WOOD_PROXY} on {path} to {target}")


def delete_unused():
    for package in renames.DELETE:
        if not ue.EditorAssetLibrary.does_asset_exist(package):
            continue
        users = [str(r) for r in ue.EditorAssetLibrary.find_package_referencers_for_asset(package, False) if str(r) != package]
        if users:
            ue.log_warning(f"Keeping {package}: used by {users}")
        elif ue.EditorAssetLibrary.delete_asset(package):
            ue.log(f"Deleted unused {package}")
    generated_folders.remove_unreferenced(ue.EditorAssetLibrary, [VEGETATION_RUNS], log=ue.log)


def remove_empty_folders():
    """Old type folders left empty by the moves, in the editor and on disk."""
    for mount, root in MOUNTS.items():
        for folder in sorted((p for p in root.rglob("*") if p.is_dir()), key=lambda p: len(p.parts), reverse=True):
            if not any(folder.rglob("*.uasset")) and not any(folder.rglob("*.umap")) and not any(folder.iterdir()):
                content = f"{mount}/{folder.relative_to(root).as_posix()}"
                ue.EditorAssetLibrary.delete_directory(content)
                if folder.exists():
                    folder.rmdir()


def ground_surface(actor):
    """The OpenFairway surface a ground actor shows, from its physical material override (PM_<Surface>)."""
    if not isinstance(actor, ue.StaticMeshActor):
        return None
    body = actor.static_mesh_component.get_editor_property("body_instance")
    material = body.get_editor_property("phys_material_override")
    name = material.get_name() if material else ""
    return name[3:] if name.startswith("PM_") else None


def relabel(map_path, course, apply):
    if not ue.get_editor_subsystem(ue.LevelEditorSubsystem).load_level(map_path):
        raise RuntimeError(f"Could not load {map_path}")
    actors = ue.get_editor_subsystem(ue.EditorActorSubsystem).get_all_level_actors()
    planned = []
    for actor in sorted(actors, key=lambda a: (a.get_actor_label(), str(a.get_actor_location()))):
        kind = actor.get_class().get_name()
        label = renames.actor_label(actor.get_actor_label(), kind, ground_surface(actor), course)
        planned.append((actor, label))
    existing = {actor.get_actor_label() for actor, label in planned if label == actor.get_actor_label()}
    numbered = renames.number_labels([label for _, label in planned if label != _.get_actor_label()], existing)
    changes = iter(numbered)
    for actor, label in planned:
        if label == actor.get_actor_label():
            continue
        label = next(changes)
        ue.log(f"{'Relabel' if apply else 'Would relabel'} {actor.get_actor_label()!r} -> {label!r}")
        if apply:
            actor.set_actor_label(label)
    if apply and not ue.get_editor_subsystem(ue.LevelEditorSubsystem).save_current_level():
        raise RuntimeError(f"Could not save {map_path}")


def main(argv):
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--apply", action="store_true")
    parser.add_argument("--quit", action="store_true", help="Close the editor afterwards (headless runs)")
    args = parser.parse_args(argv)
    try:
        # A script run at startup can start before the registry has scanned the content; renames need all of it.
        registry().wait_for_completion()
        if ue.get_editor_subsystem(ue.UnrealEditorSubsystem).get_game_world():
            raise RuntimeError("Stop Play first")
        if ue.EditorLoadingAndSavingUtils.get_dirty_map_packages() or ue.EditorLoadingAndSavingUtils.get_dirty_content_packages():
            raise RuntimeError("Save or discard editor changes first")
        moves = renames.plan(packages())
        for old, new in moves.items():
            ue.log(f"{'Move' if args.apply else 'Would move'} {old} -> {new}")
        if args.apply:
            if moves:  # A rerun after the moves only finishes the clean-up; the first run's backup covers it.
                backup()
            # A loaded map cannot be renamed; start from an empty one.
            ue.EditorLoadingAndSavingUtils.new_blank_map(False)
            rename(moves)
            fix_redirectors()
            remap_metadata()
            delete_unused()
            ue.EditorAssetLibrary.save_directory("/Game", only_if_is_dirty=True, recursive=True)
            for mount in MOUNTS:
                ue.EditorAssetLibrary.save_directory(mount, only_if_is_dirty=True, recursive=True)
            remove_empty_folders()
        # Before the rename has been applied, a dry run reads the map under its old name.
        old_map = "/Game/UOpenGolf/Maps/UOpenGolf"
        relabel(RANGE_MAP if ue.EditorAssetLibrary.does_asset_exist(RANGE_MAP) else old_map, "SavannaRange", args.apply)
        ue.log(f"OPEN_GOLF_CONTENT_RENAME_COMPLETE moves={len(moves)} apply={args.apply}")
    except Exception as error:  # Logged for headless runs, whose exit code does not carry Python errors.
        ue.log_error(f"OPEN_GOLF_CONTENT_RENAME_FAILED {error}")
        raise
    finally:
        if args.quit:
            ue.SystemLibrary.quit_editor()


if __name__ == "__main__":
    main(sys.argv[1:])
