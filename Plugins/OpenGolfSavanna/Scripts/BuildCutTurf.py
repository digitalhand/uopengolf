# SPDX-License-Identifier: MIT
"""Import the original cut turf into new assets. Existing course materials are preserved."""
import json
from pathlib import Path
import sys

import unreal as ue

HERE = Path(__file__).resolve().parent
PROJECT = HERE.parents[2]
sys.path.insert(0, str(PROJECT / 'Plugins/OpenGolfCourse/Scripts'))
sys.path.insert(0, str(HERE))
import course_assets as a
import BuildTerrainAssets as terrain
import BuildSurfaceTextures as surfaces
import savanna_assets as s
import turf_shading as shading
import turf_field as field

SOURCE = PROJECT / 'Saved/AssetImport/OpenGolfSavanna/CutTurf'
OWNER = 'OpenGolfSavanna.CutTurf.v1'
CONTENT = PROJECT / 'Plugins/OpenGolfSavanna/Content'


# Short blades read as texture beyond a few metres: they are drawn near the camera only, over ground of the
# same colour. Instances cull from three quarters of this distance.
CULL_DISTANCE_M = {'Fairway': 10, 'FirstCut': 14, 'Collar': 10}


def blade_material():
    return shading.build_blade_material(a.material('Materials/CutTurf', 'M_CutTurfBlades'))


def set_cull_distances():
    for family, distance in CULL_DISTANCE_M.items():
        preset = ue.load_asset(f'/OpenGolfSavanna/Terrain/CutTurf/DA_Turf{family}Grass')
        if preset:
            preset.set_editor_property('cull_distance_m', distance)
            a.save(preset)


def first_cut_width():
    rough = ue.load_asset('/OpenGolfSavanna/Materials/CutTurf/MI_TurfRough')
    width = a.MAT.get_material_instance_scalar_parameter_value(rough, 'FirstCutWidthM') if rough else 0
    return width if width > 0 else 1.0


def import_maps(family):
    textures = {}
    for role in ('Color', 'NormalDX', 'Roughness'):
        name = f'T_Turf{family}_{role}'
        texture = a.import_asset(name+'.png', f'Textures/CutTurf/{family}', name)
        texture.set_editor_property('srgb', role == 'Color')
        if role == 'NormalDX':
            texture.set_editor_property('compression_settings', ue.TextureCompressionSettings.TC_NORMALMAP)
        elif role == 'Roughness':
            texture.set_editor_property('compression_settings', ue.TextureCompressionSettings.TC_MASKS)
        textures[role] = a.save(texture)
    return textures


def surface_preset(family, parent, textures, color):
    instance = a.material_instance('Materials/CutTurf', f'MI_Turf{family}', parent)
    for role, parameter in surfaces.TEXTURE_ROLES.items():
        a.MAT.set_material_instance_texture_parameter_value(instance, parameter, textures[role])
    for key, value in {'TextureSizeCm': 50, 'NormalStrength': .03 if family == 'Green' else .08,
                       'FairwayReliefStrength': .18 if family == 'Green' else .55,
                       'DetailStrength': .75, 'StripeContrast': .06 if family == 'Fairway' else 0,
                       'Roughness': .90, 'Specular': .08, 'MacroStrength': .03 if family == 'Green' else .08}.items():
        a.MAT.set_material_instance_scalar_parameter_value(instance, key, value)
    for key in ('TargetColor', 'TextureMean'):
        a.MAT.set_material_instance_vector_parameter_value(instance, key, ue.LinearColor(*a.linear_color(color)))
    physical = {'Fairway':'Fairway', 'FirstCut':'Rough', 'Collar':'Fringe', 'Green':'Green'}[family]
    instance.set_editor_property('override_phys_material', True)
    instance.set_editor_property('phys_material', ue.load_asset('/OpenGolfCourse/Surfaces/PM_'+physical))
    a.MAT.update_material_instance(instance)
    a.save(instance)
    preset = terrain.create_data('DA_Turf'+family, ue.OpenGolfSurfacePreset, 'Terrain/CutTurf')
    preset.set_editor_property('surface', getattr(ue.OpenFairwaySurface, physical.upper()))
    preset.set_editor_property('material', instance)
    return a.save(preset)


def vegetation(family, meshes, width):
    preset = terrain.create_data('DA_Turf'+family+'Grass', ue.OpenGolfVegetationPreset, 'Terrain/CutTurf')
    settings = {'kind': ue.OpenGolfVegetationKind.GROUND_COVER, 'placement_pattern': ue.OpenGolfPlacementPattern.TILED,
                'surface': {'Fairway':ue.OpenFairwaySurface.FAIRWAY, 'FirstCut':ue.OpenFairwaySurface.ROUGH,
                            'Collar':ue.OpenFairwaySurface.FRINGE}[family],
                'boundary_clearance_m':0, 'tile_sink_m':.0005, 'cull_distance_m':CULL_DISTANCE_M[family],
                'maximum_slope_deg':35, 'maximum_tile_tilt_deg':3, 'scale_range':ue.Vector2D(1,1), 'seed':1731,
                'use_distance_surface':family=='FirstCut', 'distance_surface':ue.OpenFairwaySurface.FAIRWAY,
                'maximum_distance_from_play_m':width if family=='FirstCut' else 0}
    for key, value in settings.items():
        preset.set_editor_property(key, value)
    tiles = []
    for size, label in ((.5,'50cm'),(.25,'25cm'),(.125,'125mm'),(.0625,'625mm')):
        tile = ue.OpenGolfGrassTile()
        tile.set_editor_property('size_m', size)
        tile.set_editor_property('meshes', [meshes[f'SM_Turf{family}_{label}_{v}'] for v in 'ABCD'])
        tiles.append(tile)
    preset.set_editor_property('tiles',tiles)
    return a.save(preset)


def build(width=1.0, textures_only=False, refresh=False):
    manifest = json.loads((SOURCE/'export.json').read_text())
    required = [SOURCE/f'T_Turf{f}_{r}.png' for f in manifest['colors'] for r in ('Color','NormalDX','Roughness')]
    missing = [str(p) for p in required if not p.exists()]
    if missing:
        raise RuntimeError('BakeCutTurf.py must finish first: '+', '.join(missing))
    if not 0 < width <= 5:
        raise ValueError('First cut width must be between zero and five metres')
    if not textures_only and not hasattr(ue.OpenGolfTerrainDefinition, 'distance_from_surface'):
        raise RuntimeError('Restart the editor with the built native turf controls before importing')
    previous_source, previous_parent, previous_folder = a.SOURCE, surfaces.PARENT, terrain.FOLDER
    try:
        with s.course_helpers('/OpenGolfSavanna', CONTENT, OWNER):
            a.SOURCE = SOURCE
            textures = {f: import_maps(f) for f in manifest['colors']}
            if textures_only:
                ue.log('OPEN_GOLF_CUT_TURF_TEXTURES_COMPLETE')
                return
            if not refresh:
                surfaces.PARENT = 'M_CutTurfSurface'
                parent = surfaces.build_parent(textures['Fairway'])
                for family, color in manifest['colors'].items():
                    surface_preset(family, parent, textures[family], color)
            parent = ue.load_asset('/OpenGolfSavanna/Materials/M_CutTurfSurface')
            a.output(parent, shading.surface_color(parent), ue.MaterialProperty.MP_BASE_COLOR)
            parent.set_editor_property('tangent_space_normal', False)
            a.output(parent, field.surface_normal(parent, 'Fairway', 'TurfNormal'), ue.MaterialProperty.MP_NORMAL)
            a.finish_material(parent)
            blade = blade_material()
            instances = {}
            meshes = {}
            for entry in manifest['meshes']:
                family = entry['family']
                if family not in instances:
                    instance = a.material_instance('Materials/CutTurf', 'MI_Turf'+family+'Blades', blade)
                    instances[family] = a.save(instance)
                mesh = a.import_asset(entry['name']+'.fbx','Meshes/CutTurf/'+family,entry['name'],mesh=True,vertex_colors=True)
                mesh.set_material(0,instances[family])
                nanite = mesh.get_editor_property('nanite_settings')
                nanite.set_editor_property('enabled',True)
                nanite.set_editor_property('shape_preservation',ue.NaniteShapePreservation.PRESERVE_AREA)
                ue.get_editor_subsystem(ue.StaticMeshEditorSubsystem).set_nanite_settings(mesh,nanite,True)
                meshes[entry['name']] = a.save(mesh)
            if not refresh:
                for family in ('Fairway','FirstCut','Collar'):
                    vegetation(family,meshes,width)
            set_cull_distances()
            world=ue.get_editor_subsystem(ue.UnrealEditorSubsystem).get_editor_world()
            if world and world.get_path_name().split('.')[0]=='/Game/UOpenGolf/Maps/UOpenGolf':
                areas=[actor for actor in ue.get_editor_subsystem(ue.EditorActorSubsystem).get_all_level_actors() if isinstance(actor,ue.OpenGolfTerrainArea)]
                # Until FixTurfTransitions.py bakes the course field, the instances keep their previous settings.
                if len(areas)==1 and (SOURCE/'T_CourseSurfaceField.json').exists() and ue.load_asset(shading.field.FIELD_PATH):
                    shading.configure_all(areas[0], SOURCE, first_cut_width())
            ue.log('OPEN_GOLF_CUT_TURF_REFRESH_COMPLETE' if refresh else 'OPEN_GOLF_CUT_TURF_BUILD_COMPLETE')
    finally:
        a.SOURCE, surfaces.PARENT, terrain.FOLDER = previous_source, previous_parent, previous_folder


if __name__ == '__main__':
    build(textures_only='--textures-only' in sys.argv, refresh='--refresh' in sys.argv)
