# SPDX-License-Identifier: MIT
"""Remove the fairway's first cut from an applied range, so the fairway meets the 50 mm rough directly and only the
green keeps a fringe. Terrain, physics and outlines are unchanged: the first cut always played as rough.

    py "<project>/Plugins/OpenGolfSavanna/Scripts/FixTurfTransitions.py"
    py "<project>/Plugins/OpenGolfSavanna/Scripts/RemoveFirstCut.py"

Open /Game/UOpenGolf/Maps/L_SavannaRange with Play stopped. FixTurfTransitions.py must run first: it rebuilds the turf
materials without the first-cut layer and gives the fringe the first cut's colour. This script backs up the presets
and instance it changes, re-creates DA_TurfRoughGrass so the rough reaches the fairway, drops DA_TurfFirstCutGrass
from the course grass and re-places it. The first-cut presets, instances and meshes stay on disk as a rollback. The
map is saved when placement finishes; OPEN_GOLF_FIRST_CUT_REMOVED confirms the end.

If placement succeeded but the bake stopped before saving, run it again with --finish: it bakes and saves the grass
already placed, without placing it again.
"""
from datetime import datetime, timezone
import importlib
from pathlib import Path
import shutil
import sys

import unreal as ue

HERE = Path(__file__).resolve().parent
PROJECT = HERE.parents[2]
sys.path.insert(0, str(HERE))
sys.path.insert(0, str(PROJECT / 'Plugins/OpenGolfCourse/Scripts'))
import ApplyCourseGrass as course
import ApplyCutTurf as apply_turf
import BuildCutTurf as turf
import savanna_assets as s
import turf_field as field
import turf_shading as shading

TERRAIN = '/OpenGolfSavanna/CutTurf/Presets/'
FIRST_CUT_GRASS = TERRAIN + 'DA_TurfFirstCutGrass'
ROUGH_GRASS = TERRAIN + 'DA_TurfRoughGrass'
SURFACE_MATERIAL = '/OpenGolfSavanna/CutTurf/Materials/M_CutTurfSurface'
BACKUP = (FIRST_CUT_GRASS, ROUGH_GRASS, shading.MATERIALS + 'MI_TurfRough')


def backup():
    folder = PROJECT / 'Saved/TurfAssetBackups' / datetime.now(timezone.utc).strftime('%Y%m%d-%H%M%S-before-no-first-cut')
    for path in BACKUP:
        source = course.package_file(path, '.uasset')
        if source.exists():
            target = folder / (path.strip('/') + '.uasset')
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(source, target)
    return folder


def check_ready():
    """The turf materials must already draw the field without its first-cut layer."""
    if ue.get_editor_subsystem(ue.UnrealEditorSubsystem).get_game_world():
        raise RuntimeError('Stop Play before removing the first cut')
    material = s.required(SURFACE_MATERIAL)
    color = next((node for node in ue.MaterialEditingLibrary.get_material_expressions(material)
                  if node.get_editor_property('desc') == 'TurfFieldColor'), None)
    if color is None:
        raise RuntimeError(f'{SURFACE_MATERIAL} has no turf field; run FixTurfTransitions.py first')
    pins = [pin.get_editor_property('input_name') for pin in color.get_editor_property('inputs')]
    retired = [family for family in field.RETIRED if family in pins]
    if retired:
        raise RuntimeError(f'Run FixTurfTransitions.py first: {SURFACE_MATERIAL} still draws {", ".join(retired)}')


def presets_without_first_cut(area):
    """The installed course grass without the first cut, in its order. A second run finds it already removed."""
    current = [preset.get_path_name().split('.')[0] for preset in area.course_grass_presets if preset]
    if ROUGH_GRASS not in current:
        raise RuntimeError(f'Expected {ROUGH_GRASS} among the course grass presets; found {current}. '
                           'Install the 50 mm rough with ApplyCutTurfRough.py first')
    return [path for path in current if path != FIRST_CUT_GRASS]


def remove(finish=False):
    global course, field, shading, turf, apply_turf
    course = importlib.reload(course)
    field = importlib.reload(field)
    shading = importlib.reload(shading)
    turf = importlib.reload(turf)
    apply_turf = importlib.reload(apply_turf)
    area = course.terrain_area(apply_turf.MAP)
    check_ready()
    grass = presets_without_first_cut(area)
    folder = 'kept from the placement run' if finish else backup()
    if not finish:
        with s.course_helpers('/OpenGolfSavanna', turf.CONTENT, turf.OWNER):
            # Re-created from BuildCutTurf.ROUGH_BAND: no fairway clearance, so the rough reaches the fairway's edge.
            apply_turf.rough_grass()

    def finished():
        ue.log(f'OPEN_GOLF_FIRST_CUT_REMOVED presets={grass} backup={folder}')

    # No boundary preset: the 50 mm rough meets the fairway directly, like a mowing line.
    course.apply(apply_turf.MAP, apply_turf.GENERATED, grass, None, finished=finished, rebuild=not finish)


if __name__ == '__main__':
    remove(finish='--finish' in sys.argv)
