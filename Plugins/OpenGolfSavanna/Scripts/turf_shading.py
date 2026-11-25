# SPDX-License-Identifier: MIT
"""Wire the shared turf field into the cut-turf ground and blade materials, and configure their instances."""
import json
from pathlib import Path

import unreal as ue
import course_assets as a
import turf_field as field
import turf_palette as palette

MATERIALS = '/OpenGolfSavanna/CutTurf/Materials/'
# The fairway's first cut was removed on 2026-11-23; its MI_TurfFirstCut* instances and presets stay on disk as a
# rollback and are no longer configured.
GROUND_FAMILIES = ('Fairway', 'Collar', 'Green')
# The rough's ground is MI_TurfRough (ApplyCutTurf.py), so Rough has blades but no surface preset.
BLADE_FAMILIES = ('Fairway', 'Collar', 'Rough')
# Per-blade tone fades out before the instances cull, so the last visible blades match the ground.
BLADE_FADE = {'FadeStartCm': 400, 'FadeEndCm': 700, 'FadeJitterCm': 100}
# Base-to-tip gradient along BladeHeight red. These defaults leave a blade's colour unchanged.
BLADE_GRADIENT = {'BladeThatchTint': (1, 1, 1, 1), 'BladeTipTint': (1, 1, 1, 1), 'BladeTipAmount': 0,
                  'BladeBaseEnd': .35, 'BladeTipStart': .6}


def _rough_blades():
    thatch, tip = palette.rough_blade_tints()
    return {'FadeStartCm': 800, 'FadeEndCm': 1400, 'FadeJitterCm': 200,
            'BladeThatchTint': (*thatch, 1), 'BladeTipTint': (*tip, 1),
            'BladeTipAmount': palette.ROUGH_BLADE['TipAmount'], 'BladeBaseEnd': palette.ROUGH_BLADE['BaseEnd'],
            'BladeTipStart': palette.ROUGH_BLADE['TipStart']}


# Per-family overrides of BLADE_FADE and BLADE_GRADIENT. The 50 mm rough reads farther than the short cuts.
FAMILY_BLADES = {'Rough': _rough_blades()}


def surface_color(material):
    """Course ground (fairway, collar, green meshes): the field, with the rough beyond the fairway."""
    return field.field_color(material)


def blade_color(material):
    color = field.field_color(material)
    vertices = a.node(material, ue.MaterialExpressionVertexColor, 'BladeVertexColor')
    position = a.node(material, ue.MaterialExpressionWorldPosition, 'TurfWorldPosition')
    fade = a.custom(material, 'TurfBladeFade', 'float j=saturate(V.g)*J; return smoothstep(S+j,E+j,distance(P,C));',
                    {'P': (position, ''), 'V': (vertices, ''),
                     'C': (a.node(material, ue.MaterialExpressionCameraPositionWS, 'TurfCamera'), ''),
                     **{pin: (a.scalar(material, name, BLADE_FADE[name], 'Distance'), '')
                        for pin, name in (('S', 'FadeStartCm'), ('E', 'FadeEndCm'), ('J', 'FadeJitterCm'))}},
                    ue.CustomMaterialOutputType.CMOT_FLOAT1)
    gradient = {pin: (a.vector(material, name, BLADE_GRADIENT[name], 'Blade'), 'RGB')
                for pin, name in (('Th', 'BladeThatchTint'), ('Tip', 'BladeTipTint'))}
    gradient.update({pin: (a.scalar(material, name, BLADE_GRADIENT[name], 'Blade'), '')
                     for pin, name in (('TA', 'BladeTipAmount'), ('BE', 'BladeBaseEnd'), ('TS', 'BladeTipStart'))})
    # The gradient fades with the per-blade tone, so far blades still match the ground.
    return a.custom(material, 'TurfBladeColor',
                    'float variation=lerp(0.90,1.10,V.g)*lerp(0.98,1.02,V.b);'
                    'float3 g=lerp(Th,1.0,smoothstep(0.0,max(BE,0.001),V.r));'
                    'g=lerp(g,Tip,TA*smoothstep(TS,1.0,V.r));'
                    'return C*lerp(variation*g,1.0,F);',
                    {'C': (color, ''), 'V': (vertices, ''), 'F': (fade, ''), **gradient},
                    ue.CustomMaterialOutputType.CMOT_FLOAT3)


def build_blade_material(material):
    """Rebuilt from an empty graph: upward world normals on both faces, high roughness, no vertex motion."""
    a.MAT.delete_all_material_expressions(material)
    for key, value in {'two_sided': True, 'tangent_space_normal': False, 'used_with_instanced_static_meshes': True}.items():
        material.set_editor_property(key, value)
    a.output(material, blade_color(material), ue.MaterialProperty.MP_BASE_COLOR)
    a.output(material, a.node(material, ue.MaterialExpressionConstant3Vector, 'Up', constant=ue.LinearColor(0, 0, 1, 1)),
             ue.MaterialProperty.MP_NORMAL)
    a.output(material, a.scalar(material, 'Roughness', .94, 'Turf'), ue.MaterialProperty.MP_ROUGHNESS)
    a.output(material, a.scalar(material, 'Specular', .08, 'Turf'), ue.MaterialProperty.MP_SPECULAR)
    return a.finish_material(material)


def _set_vector(instance, name, values):
    a.MAT.set_material_instance_vector_parameter_value(instance, name, ue.LinearColor(*[float(v) for v in values]))


def field_settings(area, info):
    definition = area.definition
    origin, forward = area.get_actor_location(), area.get_actor_forward_vector()
    center, half = definition.get_editor_property('level_tee_center_m'), definition.get_editor_property('level_tee_half_size_m')
    low, high = info['minimum'], info['maximum']
    ranges = [limit for _, limit in info['channels']]
    if [surface for surface, _ in info['channels']] != ['FAIRWAY', 'GREEN', 'FRINGE', 'SAND']:
        raise RuntimeError('The course field channels do not match the turf shader; bake it again with turf_mask.py')
    return {'FieldBoundsM': (low[0], low[1], high[0] - low[0], high[1] - low[1]), 'FieldRangesM': ranges,
            'FieldOriginCm': (origin.x, origin.y, origin.z, 0), 'FieldForward': (forward.x, forward.y, 0, 0),
            'TeeBoxM': (center.x, center.y, half.x, half.y)}


def configure(instance, profiles, settings):
    for family, values in profiles.items():
        for name in field.SCALARS:
            a.MAT.set_material_instance_scalar_parameter_value(instance, family + name, values[name])
        for name in field.VECTORS:
            _set_vector(instance, family + name, values[name])
        a.MAT.set_material_instance_texture_parameter_value(instance, family + 'TurfTexture', values['TurfTexture'])
    return configure_field(instance, settings)


def configure_field(instance, settings):
    """The course field and its placement, the only settings that follow the play outlines."""
    a.MAT.set_material_instance_texture_parameter_value(instance, 'CourseField', field.field_texture())
    for name, value in settings.items():
        if isinstance(value, (tuple, list)):
            _set_vector(instance, name, value)
        else:
            a.MAT.set_material_instance_scalar_parameter_value(instance, name, value)
    for name, value in field.EDGES.items():
        a.MAT.set_material_instance_scalar_parameter_value(instance, name, value)
    a.MAT.update_material_instance(instance)
    return a.save(instance)


def _instances():
    names = [f'MI_Turf{f}' for f in GROUND_FAMILIES] + [f'MI_Turf{f}Blades' for f in BLADE_FAMILIES] + ['MI_TurfRough']
    for name in names:
        instance = ue.load_asset(MATERIALS + name)
        if not instance:
            raise RuntimeError(f'Missing {MATERIALS}{name}; run BuildCutTurf.py and ApplyCutTurf.py first')
        yield name, instance


def configure_field_all(area, source):
    """After the play outlines change: every cut-turf instance reads the newly baked field. The family profiles,
    which come from BakeCutTurf.py, are kept."""
    info = json.loads((Path(source) / 'T_CourseSurfaceField.json').read_text())
    settings = field_settings(area, info)
    return [name for name, instance in _instances() if configure_field(instance, settings)]


def configure_all(area, source):
    """Give every cut-turf ground, rough and blade instance the same field and family profiles."""
    info = json.loads((Path(source) / 'T_CourseSurfaceField.json').read_text())
    profiles, settings = field.profiles(source), field_settings(area, info)
    names = []
    for name, instance in _instances():
        names.append(name)
        configure(instance, profiles, settings)
        if name.endswith('Blades'):
            family = name[len('MI_Turf'):-len('Blades')]
            for key, value in {**BLADE_FADE, **BLADE_GRADIENT, **FAMILY_BLADES.get(family, {})}.items():
                if isinstance(value, tuple):
                    _set_vector(instance, key, value)
                else:
                    a.MAT.set_material_instance_scalar_parameter_value(instance, key, value)
            a.MAT.update_material_instance(instance)
            a.save(instance)
    return names
