# SPDX-License-Identifier: MIT
"""Export original cut turf. Existing Blender edits are preserved unless --regenerate is given."""
import argparse
import json
import os
from pathlib import Path
import sys

import bpy

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
sys.path.insert(0, str(HERE.parents[2] / 'OpenGolfCourse/Scripts'))
from turf_rules import SURFACES, mesh_data
from art_root import art_root

PROJECT = HERE.parents[3]
OUT = PROJECT / 'Saved/AssetImport/OpenGolfSavanna/CutTurf'


def build_source(path):
    for obj in list(bpy.context.scene.objects):
        if obj.type == 'MESH' and obj.name.startswith('SM_Turf') and 'family' in obj:
            bpy.data.objects.remove(obj, do_unlink=True)
    for family in ('Fairway', 'FirstCut', 'Collar'):
        for size, label in (((0.5, 0.5), '50cm'), ((0.25, 0.25), '25cm'),
                            ((0.125, 0.125), '125mm'), ((0.0625, 0.0625), '625mm'), ((0.5, 0.05), 'Edge')):
            for variant in range(4):
                name = f'SM_Turf{family}_{label}_{"ABCD"[variant]}'
                vertices, faces, uvs, colors = mesh_data(family,size,1731+variant)
                mesh = bpy.data.meshes.new(name)
                mesh.from_pydata(vertices, [], faces)
                mesh.update()
                uv = mesh.uv_layers.new(name='BladeUV')
                data = mesh.color_attributes.new(name='BladeHeight', type='FLOAT_COLOR', domain='CORNER')
                for loop in mesh.loops:
                    uv.data[loop.index].uv = uvs[loop.vertex_index]
                    data.data[loop.index].color = colors[loop.vertex_index]
                obj = bpy.data.objects.new(name, mesh)
                bpy.context.scene.collection.objects.link(obj)
                obj['family'] = family
                obj['footprint_m'] = list(size)
                # Height stays editable without moving the tile's roots.
                group = bpy.data.node_groups.get('Cut Turf') or bpy.data.node_groups.new('Cut Turf', 'GeometryNodeTree')
                if not group.nodes:
                    group.interface.new_socket(name='Geometry', in_out='INPUT', socket_type='NodeSocketGeometry')
                    group.interface.new_socket(name='Geometry', in_out='OUTPUT', socket_type='NodeSocketGeometry')
                    source = group.nodes.new('NodeGroupInput')
                    target = group.nodes.new('NodeGroupOutput')
                    height = group.interface.new_socket(name='Height scale', in_out='INPUT', socket_type='NodeSocketFloat')
                    height.default_value = 1.0
                    height.min_value = 0.1
                    height.max_value = 3.0
                    position = group.nodes.new('GeometryNodeInputPosition')
                    separate = group.nodes.new('ShaderNodeSeparateXYZ')
                    combine = group.nodes.new('ShaderNodeCombineXYZ')
                    multiply = group.nodes.new('ShaderNodeMath')
                    multiply.operation = 'MULTIPLY'
                    set_position = group.nodes.new('GeometryNodeSetPosition')
                    group.links.new(position.outputs['Position'], separate.inputs[0])
                    group.links.new(separate.outputs['X'], combine.inputs['X'])
                    group.links.new(separate.outputs['Y'], combine.inputs['Y'])
                    group.links.new(separate.outputs['Z'], multiply.inputs[0])
                    group.links.new(source.outputs['Height scale'], multiply.inputs[1])
                    group.links.new(multiply.outputs[0], combine.inputs['Z'])
                    group.links.new(source.outputs['Geometry'], set_position.inputs['Geometry'])
                    group.links.new(combine.outputs[0], set_position.inputs['Position'])
                    group.links.new(set_position.outputs[0], target.inputs['Geometry'])
                obj.modifiers.new('Artist adjustments', 'NODES').node_group = group
    path.parent.mkdir(parents=True, exist_ok=True)
    bpy.ops.wm.save_as_mainfile(filepath=str(path))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--regenerate', action='store_true')
    parser.add_argument('--current', action='store_true', help='Export the open scene through MCP')
    parser.add_argument('--art-root', type=Path)
    args = parser.parse_args(sys.argv[sys.argv.index('--')+1:] if '--' in sys.argv else [])
    root = args.art_root or art_root()
    source = root / 'CutTurf/CutTurf.blend'
    if args.current:
        if Path(bpy.data.filepath) != source:
            raise RuntimeError(f'Open {source} before exporting the current scene')
    elif not source.exists() or args.regenerate:
        build_source(source)
    else:
        bpy.ops.wm.open_mainfile(filepath=str(source))
    OUT.mkdir(parents=True, exist_ok=True)
    entries = []
    for obj in sorted(bpy.context.scene.objects, key=lambda o: o.name):
        if obj.type != 'MESH' or 'family' not in obj:
            continue
        maximum_height = max(v.co.z for v in obj.data.vertices)
        colors = obj.data.color_attributes.get('BladeHeight')
        if colors is None:
            colors = obj.data.color_attributes.new(name='BladeHeight', type='FLOAT_COLOR', domain='CORNER')
            for loop in obj.data.loops:
                height = obj.data.vertices[loop.vertex_index].co.z/max(maximum_height, 1e-6)
                colors.data[loop.index].color = (height, height, height, 1)
        bpy.ops.object.select_all(action='DESELECT')
        obj.select_set(True)
        bpy.context.view_layer.objects.active = obj
        with bpy.context.temp_override(scene=bpy.context.scene, view_layer=bpy.context.scene.view_layers[0],
                                       selected_objects=[obj], selected_editable_objects=[obj], active_object=obj, object=obj):
            bpy.ops.export_scene.fbx(filepath=str(OUT / (obj.name + '.fbx')), use_selection=True,
                                     object_types={'MESH'}, global_scale=1, apply_unit_scale=True,
                                     bake_anim=False, add_leaf_bones=False, axis_forward='-Y', axis_up='Z')
        entries.append({'name': obj.name, 'family': obj['family'], 'size_m': list(obj['footprint_m']), 'height_cm': float(obj.evaluated_get(bpy.context.evaluated_depsgraph_get()).dimensions.z*100)})
    (OUT / 'export.json').write_text(json.dumps({'meshes': entries, 'colors': {k:v.color for k,v in SURFACES.items()}}, indent=2)+'\n')
    print(f'OPEN_GOLF_CUT_TURF_EXPORTED {len(entries)}', flush=True)


if __name__ == '__main__':
    main()
    os._exit(0)
