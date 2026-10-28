# SPDX-License-Identifier: MIT
"""Blend the cut turf through the shared course field, without rebuilding terrain or grass.

    py "<project>/Plugins/OpenGolfSavanna/Scripts/FixTurfTransitions.py"

Open /Game/UOpenGolf/Maps/UOpenGolf with Play stopped. First export and bake the course field:
    py "<project>/Plugins/OpenGolfSavanna/Scripts/ApplyCutTurf.py" --export-mask
    python3 Plugins/OpenGolfSavanna/Scripts/turf_mask.py \\
        Saved/AssetImport/OpenGolfSavanna/CutTurf/course-field.json \\
        Saved/AssetImport/OpenGolfSavanna/CutTurf/T_CourseSurfaceField.tga
The ground, rough and blade materials then evaluate one field, so seams and blades match the ground.
The painted tee box is hidden: the field draws the tee. Assets are backed up to Saved/TurfAssetBackups.
"""
from datetime import datetime, timezone
from pathlib import Path
import importlib
import json
import shutil
import sys

import unreal as ue

HERE = Path(__file__).resolve().parent
PROJECT = HERE.parents[2]
sys.path.insert(0, str(HERE))
sys.path.insert(0, str(PROJECT / 'Plugins/OpenGolfCourse/Scripts'))
import ApplyCourseGrass as course
import BuildCutTurf as turf
import ApplyCutTurf as apply_turf
import turf_field as field
import turf_shading as shading
import savanna_assets as s

MATERIALS = ['/OpenGolfSavanna/Materials/M_CutTurfSurface', '/OpenGolfSavanna/Materials/CutTurf/M_CutTurfBlades',
             '/OpenGolfSavanna/Materials/CutTurf/M_CutTurfRough', field.FIELD_PATH]
INSTANCES = ['/OpenGolfSavanna/Materials/CutTurf/MI_Turf' + f for f in
             ('Fairway', 'FirstCut', 'Collar', 'Green', 'Rough', 'FairwayBlades', 'FirstCutBlades', 'CollarBlades')]


def backup():
    folder = PROJECT / 'Saved/TurfAssetBackups' / datetime.now(timezone.utc).strftime('%Y%m%d-%H%M%S-before-turf-field')
    for path in MATERIALS + INSTANCES:
        source = course.package_file(path, '.uasset')
        if source.exists():
            target = folder / (path.strip('/') + '.uasset')
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(source, target)
    return folder


def hide_painted_tee():
    ranges = [actor for actor in ue.get_editor_subsystem(ue.EditorActorSubsystem).get_all_level_actors()
              if isinstance(actor, ue.UOpenGolfRange)]
    if len(ranges) != 1:
        raise RuntimeError('Expected one range controller in the public map')
    ranges[0].modify()
    ranges[0].set_editor_property('show_painted_tee_box', False)
    ranges[0].refresh_presentation()


def statistics(folder):
    report = {}
    for path in MATERIALS[:3]:
        material = ue.load_asset(path)
        stats = ue.MaterialEditingLibrary.get_statistics(material)
        report[material.get_name()] = {'vertex_instructions': stats.num_vertex_shader_instructions,
                                       'pixel_instructions': stats.num_pixel_shader_instructions,
                                       'samplers': stats.num_samplers}
    (folder / 'shader-statistics.json').write_text(json.dumps(report, indent=2))
    return report


def fix():
    global turf, apply_turf, field, shading
    field = importlib.reload(field)
    shading = importlib.reload(shading)
    turf = importlib.reload(turf)
    apply_turf = importlib.reload(apply_turf)
    area = course.terrain_area(apply_turf.MAP)
    if not hasattr(area.definition, 'level_tee_half_size_m'):
        raise RuntimeError('Rebuild and restart DebugGame with the square tee box before running this script')
    folder = backup()
    previous = turf.a.SOURCE
    try:
        with s.course_helpers('/OpenGolfSavanna', turf.CONTENT, turf.OWNER):
            turf.a.SOURCE = turf.SOURCE
            apply_turf.import_field(area.definition)
            parent = ue.load_asset('/OpenGolfSavanna/Materials/M_CutTurfSurface')
            turf.a.output(parent, shading.surface_color(parent), ue.MaterialProperty.MP_BASE_COLOR)
            parent.set_editor_property('tangent_space_normal', False)
            turf.a.output(parent, field.surface_normal(parent, 'Fairway', 'TurfNormal'), ue.MaterialProperty.MP_NORMAL)
            turf.a.finish_material(parent)
            turf.blade_material()
            width = turf.first_cut_width()
            apply_turf.rough_material(area.definition.default_surface.material, width)
            shading.configure_all(area, turf.SOURCE, width)
            for family in shading.GROUND_FAMILIES:
                instance = ue.load_asset(shading.MATERIALS + 'MI_Turf' + family)
                strength = .18 if family == 'Green' else .55
                turf.a.MAT.set_material_instance_scalar_parameter_value(instance, 'FairwayReliefStrength', strength)
                turf.a.MAT.set_material_instance_scalar_parameter_value(instance, 'Roughness', .90)
                turf.a.MAT.update_material_instance(instance)
                turf.a.save(instance)
            hide_painted_tee()
            if not ue.get_editor_subsystem(ue.LevelEditorSubsystem).save_current_level():
                raise RuntimeError('Could not save the map with the painted tee hidden')
            ue.log('OPEN_GOLF_TURF_FIELD_COMPLETE ' + str(folder) + ' ' + str(statistics(folder)))
    finally:
        turf.a.SOURCE = previous


if __name__ == '__main__':
    fix()
