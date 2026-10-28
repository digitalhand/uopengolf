# SPDX-License-Identifier: MIT
"""Apply cut turf to the open public range, preserving terrain and physical surfaces.

First export the course outlines with --export-mask, bake them with turf_mask.py into
T_CourseSurfaceField.tga, then run this script without arguments. BuildCutTurf.py must have completed. --width sets the visual fairway band in metres.
"""
import argparse
import json
from pathlib import Path
import sys

import unreal as ue

HERE = Path(__file__).resolve().parent
PROJECT = HERE.parents[2]
sys.path.insert(0, str(HERE))
sys.path.insert(0, str(PROJECT / 'Plugins/OpenGolfCourse/Scripts'))
import ApplyCourseGrass as course
import BuildCutTurf as turf
import course_assets as a
import savanna_assets as s
import turf_field as field
import turf_shading as shading

MAP = '/Game/UOpenGolf/Maps/UOpenGolf'
GENERATED = '/Game/UOpenGolf/Terrain/Savanna/CutTurf'
RAMP_STRIP = '/OpenGolfSavanna/Meshes/Grass/Medium/SM_GrassMedium_30cm_EdgeRamp_'
FIELD_SOURCE = turf.SOURCE / 'T_CourseSurfaceField.tga'
FIELD_SPEC = turf.SOURCE / 'course-field.json'


def enum_name(value):
    return str(value).split('.')[1].split(':')[0]


def mask_spec(definition):
    """Course outlines for turf_mask.py, which bakes them into the shared turf field."""
    regions = [{'surface':enum_name(r.surface.surface),'outline':[[p.x,p.y] for p in r.outline_m]}
               for r in definition.regions]
    if not any(r['surface']=='FAIRWAY' for r in regions):
        raise RuntimeError('The course needs a fairway outline for its first cut')
    return {'default_surface':enum_name(definition.default_surface.surface),'regions':regions}


def write_course_spec(definition):
    FIELD_SPEC.parent.mkdir(parents=True,exist_ok=True)
    FIELD_SPEC.write_text(json.dumps(mask_spec(definition),indent=2)+'\n')
    ue.log('OPEN_GOLF_COURSE_FIELD_EXPORTED '+str(FIELD_SPEC))


def duplicate(source, folder, name):
    path = a.asset_path(folder,name)
    result = a.check_owned(path)
    if result:
        return result
    result = a.TOOLS.duplicate_asset(name,a.MOUNT+'/'+folder,source)
    if not result:
        raise RuntimeError('Could not duplicate '+source.get_path_name())
    return a.save(result)


def import_field(definition):
    """Import the baked course field after checking it was made from the current outlines."""
    if not FIELD_SOURCE.exists() or not FIELD_SPEC.exists() or not FIELD_SOURCE.with_suffix('.json').exists():
        raise RuntimeError('Export the course outlines with --export-mask and bake them with turf_mask.py first')
    if json.loads(FIELD_SPEC.read_text()) != mask_spec(definition):
        raise RuntimeError('The course outlines changed: export and bake the course field again')
    texture = a.import_asset(str(FIELD_SOURCE),'Textures/CutTurf','T_CourseSurfaceField')
    # Uncompressed 8-bit channels keep the distance steps small; block compression would quantize the edges.
    for key,value in {'srgb':False,'compression_settings':ue.TextureCompressionSettings.TC_VECTOR_DISPLACEMENTMAP,
                       'mip_gen_settings':ue.TextureMipGenSettings.TMGS_SIMPLE_AVERAGE,
                       'address_x':ue.TextureAddress.TA_CLAMP,'address_y':ue.TextureAddress.TA_CLAMP}.items():
        texture.set_editor_property(key,value)
    return a.save(texture)


def rough_material(source, width):
    """Savanna rough beyond the first cut; the shared field draws the first cut and the fairway edge."""
    parent = duplicate(s.required('/OpenGolfSavanna/Materials/M_SavannaGround'),'Materials/CutTurf','M_CutTurfRough')
    ground = a.node(parent,ue.MaterialExpressionCustom,'GroundColor')
    a.output(parent,field.field_color(parent,(ground,'')),ue.MaterialProperty.MP_BASE_COLOR)
    normal = a.custom(parent,'TurfFieldGroundNormal','return normalize(lerp(N,T,W));',
                      {'N':(a.node(parent,ue.MaterialExpressionCustom,'GroundNormal'),''),
                       'T':(field.surface_normal(parent, 'FirstCut'), ''), 'W':(field.field_coverage(parent),'')},
                      ue.CustomMaterialOutputType.CMOT_FLOAT3)
    a.output(parent,normal,ue.MaterialProperty.MP_NORMAL)
    rough_map = a.node(parent, ue.MaterialExpressionTextureSampleParameter2D, 'FirstCutRoughnessMap',
                       parameter_name='FirstCutRoughnessMap',
                       texture=ue.load_asset('/OpenGolfSavanna/Textures/CutTurf/FirstCut/T_TurfFirstCut_Roughness'),
                       sampler_type=ue.MaterialSamplerType.SAMPLERTYPE_MASKS)
    a.connect(a.node(parent, ue.MaterialExpressionCustom, 'FirstCutReliefUV'), rough_map, 'UVs')
    roughness = a.custom(parent, 'TurfFieldGroundRoughness', 'return lerp(R,max(0.9,T),W);',
                        {'R': (a.node(parent, ue.MaterialExpressionCustom, 'GroundRoughness'), ''),
                         'T': (rough_map, 'R'), 'W': (field.field_coverage(parent), '')},
                        ue.CustomMaterialOutputType.CMOT_FLOAT1)
    a.output(parent, roughness, ue.MaterialProperty.MP_ROUGHNESS)
    a.finish_material(parent)
    instance = duplicate(source,'Materials/CutTurf','MI_TurfRough')
    a.MAT.set_material_instance_parent(instance,parent)
    a.MAT.set_material_instance_scalar_parameter_value(instance,'FirstCutWidthM',width)
    a.MAT.update_material_instance(instance)
    return a.save(instance)


def apply(width=1.0, export_mask=False):
    area = course.terrain_area(MAP)
    definition = area.definition
    if export_mask:
        write_course_spec(definition)
        return
    if not hasattr(definition,'distance_from_surface'):
        raise RuntimeError('Restart DebugGame with the new turf modules before applying')
    course.backup(MAP,definition)
    with s.course_helpers('/OpenGolfSavanna',turf.CONTENT,turf.OWNER):
        old_rough = definition.default_surface.material
        import_field(definition)
        new_rough = rough_material(old_rough,width)
        rough_preset = duplicate(definition.default_surface,'Terrain/CutTurf','DA_TurfRough')
        rough_preset.set_editor_property('material',new_rough)
        a.save(rough_preset)
        # Clone the authored rough presets so their appearance and original settings stay available.
        edge = duplicate(s.required('/OpenGolfSavanna/Terrain/DA_BoundaryGrass'),'Terrain/CutTurf','DA_TurfRoughEdge')
        edge.set_editor_property('boundary_clearance_m',0)
        edge.set_editor_property('fairway_clearance_m',width)
        # Graded 1 x 0.5 m strips rise from the first cut's height to the tall rough's.
        edge.set_editor_property('meshes',[s.required(RAMP_STRIP+v) for v in 'AB'])
        edge.set_editor_property('edge_strip_size_m',ue.Vector2D(1.0,0.5))
        a.save(edge)
        # Tall tiles start one strip deep behind the edge row: the smallest is judged at its centre, so its
        # corners would otherwise poke through the row as a staircase.
        rough = duplicate(s.required('/OpenGolfSavanna/Terrain/DA_RoughGrassDense'),'Terrain/CutTurf','DA_TurfTallRough')
        rough.set_editor_property('fairway_clearance_m',width+edge.edge_strip_size_m.y)
        a.save(rough)
        shading.configure_all(area,turf.SOURCE,width)
        first_cut = s.required('/OpenGolfSavanna/Terrain/CutTurf/DA_TurfFirstCutGrass')
        first_cut.set_editor_property('maximum_distance_from_play_m',width)
        a.save(first_cut)
    def finish():
        definition.modify()
        definition.set_editor_property('default_surface',rough_preset)
        for actor in area.generated_actors:
            if not actor:
                continue
            for component in actor.get_components_by_class(ue.StaticMeshComponent):
                if isinstance(component,ue.InstancedStaticMeshComponent):
                    continue
                for slot in range(component.get_num_materials()):
                    if component.get_material(slot)==old_rough:
                        component.modify()
                        component.set_material(slot,new_rough)
        if not ue.EditorAssetLibrary.save_loaded_asset(definition) or not ue.get_editor_subsystem(ue.LevelEditorSubsystem).save_current_level():
            raise RuntimeError('Could not save the applied turf')
        ue.log('OPEN_GOLF_CUT_TURF_APPLIED')
    grass = [rough.get_path_name()] + [f'/OpenGolfSavanna/Terrain/CutTurf/DA_Turf{f}Grass' for f in ('Fairway','FirstCut','Collar')]
    surfaces = [f'/OpenGolfSavanna/Terrain/CutTurf/DA_Turf{f}' for f in ('Fairway','Collar','Green')]
    course.apply(MAP,GENERATED,grass,edge.get_path_name(),surfaces,finished=finish)


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--export-mask',action='store_true')
    parser.add_argument('--width',type=float,default=1)
    args=parser.parse_args()
    # The course field stores fairway distance to 2.5 m; the first cut's soft outer edge must fit inside it.
    if not 0 < args.width <= 2.2:
        raise ValueError('First cut width must be between zero and 2.2 metres (see turf_mask.CHANNELS)')
    apply(args.width,args.export_mask)
