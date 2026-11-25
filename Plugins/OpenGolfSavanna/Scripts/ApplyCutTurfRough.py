# SPDX-License-Identifier: MIT
"""Replace the Poly Haven tall rough and its edge strip with the 50 mm cut-turf rough, without rebuilding terrain.

    py "<project>/Plugins/OpenGolfSavanna/Scripts/ApplyCutTurfRough.py"

Open /Game/UOpenGolf/Maps/L_SavannaRange with Play stopped. First export, bake and refresh the Rough family and
rebuild the turf field with FixTurfTransitions.py (Docs/TurfWorkflow.md). This script backs up the presets and
instances it changes, installs DA_TurfRoughGrass in place of DA_TurfTallRough, removes the boundary strip
(DA_TurfRoughEdge) and re-places the course grass. The old presets stay on disk as a rollback. The map is saved
when placement finishes; OPEN_GOLF_CUT_TURF_ROUGH_COMPLETE confirms the end.

If placement succeeded but the bake stopped before saving (for example "Course grass bake timed out"), run it again
with --finish: it bakes and saves the grass already placed, without placing it again.
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
import turf_palette as palette
import turf_shading as shading

TERRAIN = '/OpenGolfSavanna/CutTurf/Presets/'
TALL_ROUGH = TERRAIN + 'DA_TurfTallRough'
ROUGH_EDGE = TERRAIN + 'DA_TurfRoughEdge'
ROUGH_GRASS = TERRAIN + 'DA_TurfRoughGrass'
ROUGH_MATERIAL = '/OpenGolfSavanna/CutTurf/Materials/M_CutTurfRough'
BACKUP = (TALL_ROUGH, ROUGH_EDGE, ROUGH_GRASS, ROUGH_MATERIAL, shading.MATERIALS + 'MI_TurfRough',
          shading.MATERIALS + 'MI_TurfRoughBlades')


def backup():
    folder = PROJECT / 'Saved/TurfAssetBackups' / datetime.now(timezone.utc).strftime('%Y%m%d-%H%M%S-before-rough')
    for path in BACKUP:
        source = course.package_file(path, '.uasset')
        if source.exists():
            target = folder / (path.strip('/') + '.uasset')
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(source, target)
    return folder


def check_ready():
    """The Rough family must be imported and the rough ground rebuilt with the rough as its lowest field layer."""
    missing = [path for path in (shading.MATERIALS + 'MI_TurfRoughBlades',
                                 '/OpenGolfSavanna/CutTurf/Textures/Rough/T_TurfRough_D')
               if not ue.load_asset(path)]
    if missing:
        raise RuntimeError('Run BuildCutTurf.py --refresh after exporting and baking the Rough family; missing '
                           + ', '.join(missing))
    material = s.required(ROUGH_MATERIAL)
    if not any(node.get_editor_property('desc') == 'TurfRoughBand'
               for node in ue.MaterialEditingLibrary.get_material_expressions(material)):
        raise RuntimeError('Run FixTurfTransitions.py first: M_CutTurfRough does not draw the rough layer yet')


def presets_with_rough(area):
    """The installed course grass, with the tall rough swapped for the cut-turf rough and the order kept."""
    current = [preset.get_path_name().split('.')[0] for preset in area.course_grass_presets if preset]
    if ROUGH_GRASS in current:
        return [path for path in current if path != TALL_ROUGH]
    if TALL_ROUGH not in current:
        raise RuntimeError(f'Expected {TALL_ROUGH} among the course grass presets; found {current}')
    return [ROUGH_GRASS if path == TALL_ROUGH else path for path in current]


def install(finish=False):
    global course, palette, field, shading, turf, apply_turf
    course = importlib.reload(course)
    palette = importlib.reload(palette)
    field = importlib.reload(field)
    shading = importlib.reload(shading)
    turf = importlib.reload(turf)
    apply_turf = importlib.reload(apply_turf)
    area = course.terrain_area(apply_turf.MAP)
    check_ready()
    grass = presets_with_rough(area)
    folder = 'kept from the placement run' if finish else backup()
    if not finish:
        with s.course_helpers('/OpenGolfSavanna', turf.CONTENT, turf.OWNER):
            apply_turf.rough_grass()
            turf.set_cull_distances()
            shading.configure_all(area, turf.SOURCE)

    def finished():
        ue.log(f'OPEN_GOLF_CUT_TURF_ROUGH_COMPLETE presets={grass} backup={folder}')

    # No boundary preset: the 50 mm rough meets the fairway directly, like a mowing line.
    course.apply(apply_turf.MAP, apply_turf.GENERATED, grass, None, finished=finished, rebuild=not finish)


if __name__ == '__main__':
    install(finish='--finish' in sys.argv)
