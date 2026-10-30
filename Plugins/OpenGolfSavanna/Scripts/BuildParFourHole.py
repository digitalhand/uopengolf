# SPDX-License-Identifier: MIT
"""Turn the built savanna range into its par-4 hole (savanna_hole.py), keeping everything else.

    py "<project>/Plugins/OpenGolfSavanna/Scripts/BuildParFourHole.py"

Open /Game/UOpenGolf/Maps/UOpenGolf with Play stopped. The script backs up the map and terrain definition, then:

1. lays out the hole in DA_SavannaRange: the green 400 yards out with its contour, the fairway bunker on the left of
   the landing area, a greenside bunker right of the green, and the backdrop hill moved behind the new green;
2. rebuilds the ground meshes (the Landscape outside the detailed area is unchanged) and stands the flag at the pin;
3. exports and bakes the course field (turf_mask.py needs NumPy: set OPEN_GOLF_NUMPY_PYTHON to a Python that has it,
   otherwise "python" runs it), imports it and points the cut-turf materials at it (their turf profiles are kept);
4. places the course grass along the new outlines, then rebuilds the trees and understory away from them.

The map is saved after each step. Steps 1-3 finish within the call; step 4 waits for PCG, so keep the editor open
until OPEN_GOLF_RANGE_VEGETATION_COMPLETE (a run started with -ExecutePythonScript quits too early). --replant runs
only step 4. Running it again rebuilds the same hole.
"""
from pathlib import Path
import importlib
import os
import subprocess
import sys

import unreal as ue

HERE = Path(__file__).resolve().parent
PROJECT = HERE.parents[2]
sys.path.insert(0, str(HERE))
sys.path.insert(0, str(PROJECT / "Scripts"))
sys.path.insert(0, str(PROJECT / "Plugins/OpenGolfCourse/Scripts"))
import ApplyCourseGrass as course
import ApplyCutTurf as apply_turf
import BuildSavannaRange as savanna
import BuildCutTurf as turf
import RebuildRangeVegetation as vegetation
import course_hole
import savanna_assets as s
import savanna_hole as hole_spec
import turf_shading as shading

course, apply_turf, savanna, turf, vegetation, course_hole, s, hole_spec, shading = [
    importlib.reload(m) for m in (course, apply_turf, savanna, turf, vegetation, course_hole, s, hole_spec, shading)]

SURFACE_FOLDER = "/Game/UOpenGolf/Terrain/Generated"


def bake_course_field(area):
    """Exports the outlines, bakes them into the turf field with turf_mask.py, imports it and points the cut-turf
    materials at it."""
    apply_turf.write_course_spec(area.definition)
    python = os.environ.get("OPEN_GOLF_NUMPY_PYTHON", "python")
    subprocess.run([python, str(HERE / "turf_mask.py"), str(apply_turf.FIELD_SPEC), str(apply_turf.FIELD_SOURCE)], check=True)
    with s.course_helpers("/OpenGolfSavanna", turf.CONTENT, turf.OWNER):
        apply_turf.import_field(area.definition)
        shading.configure_field_all(area, turf.SOURCE, turf.first_cut_width())


def build():
    editor = ue.get_editor_subsystem(ue.UnrealEditorSubsystem)
    if editor.get_game_world():
        raise RuntimeError("Stop Play before building the hole")
    area = course.terrain_area(apply_turf.MAP)
    ranges = [a for a in ue.get_editor_subsystem(ue.EditorActorSubsystem).get_all_level_actors() if isinstance(a, ue.UOpenGolfRange)]
    if len(ranges) != 1:
        raise RuntimeError("Expected one range controller")
    definition = area.definition
    backup = course.backup(apply_turf.MAP, definition)

    definition.modify()
    course_hole.lay_out(definition, hole_spec.apply_layout, hole_spec.REGION_TEMPLATES)
    if ue.OpenGolfTerrainBuilder.rebuild_surface_meshes(area, SURFACE_FOLDER) is None:
        raise RuntimeError(f"Ground rebuild failed (see LogTemp); nothing was saved. Backup: {backup}")
    if not ue.EditorAssetLibrary.save_loaded_asset(definition):
        raise RuntimeError("Could not save the terrain definition")
    course_hole.place_flag(ranges[0], area, hole_spec.PIN)
    bake_course_field(area)
    if not ue.get_editor_subsystem(ue.LevelEditorSubsystem).save_current_level():
        raise RuntimeError(f"Could not save the map. Backup: {backup}")
    replant(area, backup)


def replant(area, backup):
    """Course grass along the outlines, then trees and understory away from them. Both wait for PCG over the next
    frames, so the editor must keep running until OPEN_GOLF_RANGE_VEGETATION_COMPLETE."""
    grass = [p.get_path_name() for p in area.course_grass_presets]
    boundary = area.boundary_grass_preset.get_path_name() if area.boundary_grass_preset else None

    def trees_finished(_range, terrain):
        result = savanna.finish_trees(terrain, backup)
        ue.log(f"OPEN_GOLF_PAR_FOUR_HOLE_COMPLETE {result}; backup={backup}")

    def start_trees(_delta):
        ue.unregister_slate_post_tick_callback(handle[0])
        vegetation.rebuild(apply_turf.MAP, vegetation.GENERATED, finished=trees_finished)

    handle = []

    def grass_finished():
        # Start on the next tick: the grass step deletes unused generated folders right after this returns, and the
        # new vegetation run would share one of them before the map references it.
        handle.append(ue.register_slate_post_tick_callback(start_trees))

    # Course grass saves the map when PCG finishes, then the trees are rebuilt away from the new outlines.
    course.apply(apply_turf.MAP, apply_turf.GENERATED, grass, boundary, cleanup=[apply_turf.GENERATED, SURFACE_FOLDER],
                 finished=grass_finished)


if __name__ == "__main__":
    if "--replant" in sys.argv[1:]:
        # Only step 4, for a hole whose layout, ground and field are already saved.
        area = course.terrain_area(apply_turf.MAP)
        replant(area, course.backup(apply_turf.MAP, area.definition))
    else:
        build()
