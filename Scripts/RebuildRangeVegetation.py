# SPDX-License-Identifier: MIT
"""Rebuild only a range map's vegetation, trees and trunk colliders from its terrain definition.
Terrain, tee and targets are kept. The map is backed up first and saved only after the bake succeeds.
Then generated folders (Build_ and Vegetation_) under --generated that nothing on disk references are
deleted, unless --keep-generated is given. Save any other open levels first, and review the log: deleted
folders are not backed up, but they stay in Git history.

    py "<project>/Scripts/RebuildRangeVegetation.py" [--keep-generated] [--map PATH --generated PATH]

--map and --generated select another range map (for example a private copy in a plugin) and the
content folder its generated vegetation is written to.
"""
from datetime import datetime, timezone
from pathlib import Path
import shutil
import sys

import unreal as ue

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "Plugins/OpenGolfCourse/Scripts"))
import generated_folders

MAP = "/Game/UOpenGolf/Maps/UOpenGolf"
GENERATED = "/Game/UOpenGolf/Terrain/Generated"
LIB = ue.EditorAssetLibrary
BAKE_TIMEOUT_S = 180


def map_file(map_path):
    """The .umap file for a /Game or plugin-mounted map package."""
    root = Path(ue.Paths.project_dir()).resolve()
    mount, _, relative = map_path.strip("/").partition("/")
    if mount == "Game":
        return root / "Content" / f"{relative}.umap"
    descriptors = list((root / "Plugins").rglob(f"{mount}.uplugin"))
    if len(descriptors) != 1:
        raise RuntimeError(f"Cannot locate the plugin that mounts {map_path}")
    return descriptors[0].parent / "Content" / f"{relative}.umap"


def vegetation_folders(generated):
    return generated_folders.run_folders(LIB, generated, prefixes=("Vegetation_",))


def rebuild(map_path=MAP, generated=GENERATED, remove=True, prepare=None, finished=None):
    """Rebuilds vegetation in map_path. prepare(range_actor, area) may adjust the level before the rebuild."""
    levels = ue.get_editor_subsystem(ue.LevelEditorSubsystem)
    actors = ue.get_editor_subsystem(ue.EditorActorSubsystem)
    if ue.get_editor_subsystem(ue.UnrealEditorSubsystem).get_game_world():
        raise RuntimeError("Stop Play before rebuilding vegetation")
    world = ue.get_editor_subsystem(ue.UnrealEditorSubsystem).get_editor_world()
    current = world.get_path_name().split(".")[0] if world else ""
    if current != map_path and not levels.load_level(map_path):
        raise RuntimeError(f"Could not open {map_path}")
    ranges = [a for a in actors.get_all_level_actors() if isinstance(a, ue.UOpenGolfRange)]
    if len(ranges) != 1 or not ranges[0].get_editor_property("terrain"):
        raise RuntimeError("Expected exactly one range controller with terrain")
    area = ranges[0].get_editor_property("terrain")
    root = Path(ue.Paths.project_dir()).resolve()
    stamp = datetime.now(timezone.utc).strftime("%Y%m%d-%H%M%S")
    backup = root / "Saved/TerrainBackups" / f"{stamp}-vegetation"
    backup.mkdir(parents=True, exist_ok=False)
    source = map_file(map_path)
    shutil.copy2(source, backup / source.name)
    if prepare:
        prepare(ranges[0], area)
    before = vegetation_folders(generated)
    if ue.OpenGolfTerrainBuilder.rebuild_vegetation(area, generated) is None:
        raise RuntimeError(f"Vegetation rebuild failed; the map was not saved. Backup: {backup}")
    created = vegetation_folders(generated) - before
    started = datetime.now(timezone.utc)
    busy = []

    # Baking, saving and scripted checks can pump Slate, which calls this again from inside itself.
    def finish(_delta):
        if busy:
            return
        busy.append(True)
        try:
            result = ue.OpenGolfTerrainBuilder.bake_vegetation(area)
            if result is None and (datetime.now(timezone.utc) - started).total_seconds() < BAKE_TIMEOUT_S:
                busy.clear()
                return
        except Exception as error:
            ue.unregister_slate_post_tick_callback(handle)
            ue.log_error(f"Vegetation bake raised {error}; map not saved. Backup: {backup}")
            return
        ue.unregister_slate_post_tick_callback(handle)
        if result is None:
            ue.log_error(f"Vegetation bake failed; map not saved. Backup: {backup}")
            return
        if finished:
            try:
                finished(ranges[0], area)
            except Exception as error:
                ue.log_error(f"Vegetation checks failed: {error}; map not saved. Backup: {backup}")
                return
        if not levels.save_current_level():
            ue.log_error("Could not save the range")
            return
        if len(created) != 1:
            ue.log_warning(f"Expected one new vegetation folder, found {sorted(created)}; no folders removed")
        elif remove:
            # Baked trees and cover reference their meshes, not the run's PCG graphs, so the new run's folder
            # goes too, along with any ground build the saved map no longer uses.
            generated_folders.remove_unreferenced(LIB, [generated], log=ue.log)
        else:
            ue.log("Generated folders kept (--keep-generated)")
        (backup / "completed.txt").write_text(f"Vegetation folders: {sorted(created)}\n", encoding="utf-8")
        ue.log("OPEN_GOLF_RANGE_VEGETATION_COMPLETE")

    handle = ue.register_slate_post_tick_callback(finish)
    ue.log(f"Vegetation started; waiting for PCG before baking. Map backup: {backup}")


def option(name, default):
    args = sys.argv[1:]
    return args[args.index(name) + 1] if name in args and args.index(name) + 1 < len(args) else default


if __name__ == "__main__":
    rebuild(option("--map", MAP), option("--generated", GENERATED), "--keep-generated" not in sys.argv[1:])
