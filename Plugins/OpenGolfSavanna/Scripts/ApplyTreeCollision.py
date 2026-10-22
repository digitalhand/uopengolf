# SPDX-License-Identifier: MIT
"""Update an open savanna course's jacaranda ball collision in place, without rebuilding its trees.

    py "<project>/Plugins/OpenGolfSavanna/Scripts/ApplyTreeCollision.py" [--map PATH] [--canopy]

Import the collision mesh first (blender/ExportJacaranda.py, then ImportSavannaAssets.py with
--meshes=SM_JacarandaCollision,SM_JacarandaCanopy). The map must be open with Play stopped; it is
backed up, every tree collider is swapped for the trunk-and-main-limb proxy, ball-sized sweeps check
that wood blocks and branches and leaves do not, and the map is saved only when the check passes.
--canopy also replaces each tree's hidden canopy volume (it slows a ball flying through the crown; it
needs the native RebuildTreeCanopy) before the check, which then also shows the canopy never blocks.
Completion is logged as OPEN_GOLF_TREE_COLLISION_COMPLETE.
"""
from datetime import datetime, timezone
from pathlib import Path
import argparse
import importlib
import json
import shutil
import sys

import unreal as ue

PROJECT = Path(ue.Paths.project_dir()).resolve()
HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
sys.path.insert(0, str(PROJECT / "Scripts"))
import RebuildRangeVegetation as vegetation
import savanna_trees as trees

vegetation, trees = [importlib.reload(m) for m in (vegetation, trees)]


def terrain_area(map_path):
    editor = ue.get_editor_subsystem(ue.UnrealEditorSubsystem)
    world = editor.get_editor_world()
    if editor.get_game_world() or not world or world.get_path_name().split(".")[0] != map_path:
        raise RuntimeError(f"Open {map_path} and stop Play before applying tree collision")
    areas = [a for a in ue.get_editor_subsystem(ue.EditorActorSubsystem).get_all_level_actors()
             if isinstance(a, ue.OpenGolfTerrainArea)]
    if len(areas) != 1:
        raise RuntimeError("Expected one terrain area in this map")
    return areas[0]


def backup(map_path, label):
    folder = PROJECT / "Saved/TerrainBackups" / f"{datetime.now(timezone.utc).strftime('%Y%m%d-%H%M%S')}-{label}"
    folder.mkdir(parents=True, exist_ok=False)
    source = vegetation.map_file(map_path)
    shutil.copy2(source, folder / source.name)
    return folder


def main(argv):
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--map", default=vegetation.MAP)
    parser.add_argument("--canopy", action="store_true")
    args = parser.parse_args(argv)
    area = terrain_area(args.map)
    folder = backup(args.map, "tree-collision")
    try:
        canopy = trees.apply_canopy(area) if args.canopy else None
        report = trees.apply(area, folder / "collision.json")
    except Exception as error:
        raise RuntimeError(f"{error}; map not saved. Backup: {folder}") from error
    if not ue.get_editor_subsystem(ue.LevelEditorSubsystem).save_current_level():
        raise RuntimeError(f"Could not save the map. Backup: {folder}")
    summary = {key: report[key] for key in ("trees", "wood_hits", "expected_wood_hits", "clear_leaf_sweeps", "expected_clear")}
    summary["canopy_volumes"] = canopy
    ue.log(f"OPEN_GOLF_TREE_COLLISION_COMPLETE {json.dumps(summary)}; backup={folder}")


if __name__ == "__main__":
    main(sys.argv[1:])
