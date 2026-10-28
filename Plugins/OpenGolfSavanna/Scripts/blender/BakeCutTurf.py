# SPDX-License-Identifier: MIT
"""Bake unlit, repeating turf colour and height-derived normals from the original blades."""
import json
from pathlib import Path
import sys
import os
import bpy
import numpy as np
from mathutils import Vector

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
sys.path.insert(0, str(HERE.parents[2] / 'OpenGolfCourse/Scripts'))
from art_root import art_root
from turf_rules import SURFACES, mesh_data
import png8

OUT = HERE.parents[3] / 'Saved/AssetImport/OpenGolfSavanna/CutTurf'


def emission(name, color):
    material = bpy.data.materials.new(name)
    material.use_nodes = True
    nodes = material.node_tree.nodes
    nodes.clear()
    output = nodes.new('ShaderNodeOutputMaterial')
    shader = nodes.new('ShaderNodeEmission')
    shader.inputs['Color'].default_value = (*color, 1)
    material.node_tree.links.new(shader.outputs[0], output.inputs[0])
    return material, shader


def record_mean(family, path):
    """Store the colour bake's linear mean, so Unreal scales the texture to exactly the target colour."""
    image = bpy.data.images.load(str(path), check_existing=False)
    image.colorspace_settings.name = 'Non-Color'
    pixels = np.empty(len(image.pixels), dtype=np.float32)
    image.pixels.foreach_get(pixels)
    bpy.data.images.remove(image)
    rgb = pixels.reshape(-1, 4)[:, :3].astype(np.float64)
    linear = np.where(rgb <= .04045, rgb/12.92, ((rgb+.055)/1.055)**2.4)
    target = OUT / 'texture-means.json'
    means = json.loads(target.read_text()) if target.exists() else {}
    means[family] = [round(float(v), 6) for v in linear.mean(0)]
    target.write_text(json.dumps(means, indent=2, sort_keys=True) + '\n')


def bake(family):
    spec = SURFACES[family]
    scene = bpy.data.scenes.new('Bake ' + family)
    scene.render.engine = 'CYCLES'
    scene.cycles.samples = 1
    scene.render.resolution_x = scene.render.resolution_y = 2048
    scene.render.resolution_percentage = 100
    scene.render.image_settings.file_format = 'PNG'
    scene.render.image_settings.color_depth = '8'
    scene.render.image_settings.color_mode = 'RGB'
    scene.view_settings.view_transform = 'Standard'
    scene.view_settings.look = 'None'
    scene.render.film_transparent = False
    color = [int(spec.color[i:i+2], 16)/255 for i in (0, 2, 4)]
    color = [c/12.92 if c <= .04045 else ((c+.055)/1.055)**2.4 for c in color]
    blade_material, blade_shader = emission('Bake Blade ' + family, color)
    ground_material, ground_shader = emission('Bake Ground ' + family, [c*.83 for c in color])
    noise = ground_material.node_tree.nodes.new('ShaderNodeTexNoise')
    noise.inputs['Scale'].default_value = {'Fairway':250, 'FirstCut':150, 'Collar':200, 'Green':1000}[family]
    noise.inputs['Detail'].default_value = 2
    ground_geometry = ground_material.node_tree.nodes.new('ShaderNodeNewGeometry')
    ground_material.node_tree.links.new(ground_geometry.outputs['Position'], noise.inputs['Vector'])
    ramp = ground_material.node_tree.nodes.new('ShaderNodeValToRGB')
    ramp.color_ramp.elements[0].color = (*[c*.68 for c in color],1)
    ramp.color_ramp.elements[1].color = (*[c*1.05 for c in color],1)
    ground_material.node_tree.links.new(noise.outputs['Fac'],ramp.inputs[0])
    ground_material.node_tree.links.new(ramp.outputs['Color'],ground_shader.inputs['Color'])
    attribute = blade_material.node_tree.nodes.new('ShaderNodeVertexColor')
    attribute.layer_name = 'BladeHeight'
    channels = blade_material.node_tree.nodes.new('ShaderNodeSeparateColor')
    blade_material.node_tree.links.new(attribute.outputs['Color'],channels.inputs['Color'])
    tone = blade_material.node_tree.nodes.new('ShaderNodeMapRange')
    tone.inputs['To Min'].default_value = .82
    tone.inputs['To Max'].default_value = 1.14
    blade_material.node_tree.links.new(channels.outputs['Green'],tone.inputs['Value'])
    tint = blade_material.node_tree.nodes.new('ShaderNodeVectorMath')
    tint.operation = 'SCALE'
    tint.inputs[0].default_value = color
    blade_material.node_tree.links.new(tone.outputs['Result'],tint.inputs['Scale'])
    blade_material.node_tree.links.new(tint.outputs['Vector'],blade_shader.inputs['Color'])
    geometry = blade_material.node_tree.nodes.new('ShaderNodeNewGeometry')
    separate = blade_material.node_tree.nodes.new('ShaderNodeSeparateXYZ')
    blade_material.node_tree.links.new(geometry.outputs['Position'], separate.inputs[0])
    scale = blade_material.node_tree.nodes.new('ShaderNodeMath')
    scale.operation = 'DIVIDE'
    scale.inputs[1].default_value = spec.height*1.1
    blade_material.node_tree.links.new(separate.outputs['Z'], scale.inputs[0])
    size = .5
    vertices, faces, uvs, colors = mesh_data(family,(size,size),1731)
    source = bpy.data.objects.get(f'SM_Turf{family}_50cm_A')
    if source is not None:
        mesh = bpy.data.meshes.new_from_object(source.evaluated_get(bpy.context.evaluated_depsgraph_get()))
        mesh.materials.clear()
    else:
        if family != 'Green':
            raise RuntimeError('Open the saved CutTurf.blend before baking')
        mesh = bpy.data.meshes.new('Bake Blades ' + family)
        mesh.from_pydata(vertices, [], faces)
        data = mesh.color_attributes.new(name='BladeHeight', type='FLOAT_COLOR', domain='CORNER')
        for loop in mesh.loops:
            data.data[loop.index].color = colors[loop.vertex_index]
    mesh.materials.append(blade_material)
    for x in (-size, 0, size):
        for y in (-size, 0, size):
            obj = bpy.data.objects.new('Repeat', mesh)
            obj.location = (x,y,0)
            scene.collection.objects.link(obj)
    ground = bpy.data.meshes.new('Bake Ground')
    ground.from_pydata([(-1,-1,-.0001),(1,-1,-.0001),(1,1,-.0001),(-1,1,-.0001)], [], [(0,1,2,3)])
    ground.materials.append(ground_material)
    scene.collection.objects.link(bpy.data.objects.new('Ground', ground))
    camera = bpy.data.cameras.new('Bake Camera')
    camera.type = 'ORTHO'
    camera.ortho_scale = size
    camera_obj = bpy.data.objects.new('Bake Camera', camera)
    camera_obj.location = (0,0,1)
    camera_obj.rotation_euler = (Vector((0,0,0))-camera_obj.location).to_track_quat('-Z','Y').to_euler()
    scene.collection.objects.link(camera_obj)
    scene.camera = camera_obj
    OUT.mkdir(parents=True, exist_ok=True)
    scene.render.filepath = str(OUT / f'T_Turf{family}_Color.png')
    bpy.ops.render.render(scene=scene.name, write_still=True)
    record_mean(family, Path(scene.render.filepath))
    blade_material.node_tree.links.new(scale.outputs[0], blade_shader.inputs['Color'])
    ground_material.node_tree.links.remove(ground_shader.inputs['Color'].links[0])
    ground_shader.inputs['Color'].default_value = (0,0,0,1)
    scene.view_settings.view_transform = 'Raw'
    scene.render.image_settings.file_format = 'OPEN_EXR'
    scene.render.image_settings.color_depth = '32'
    height_folder = art_root() / 'CutTurf/Bakes'
    height_folder.mkdir(parents=True, exist_ok=True)
    scene.render.filepath = str(height_folder / f'T_Turf{family}_Height.exr')
    bpy.ops.render.render(scene=scene.name, write_still=True)
    render = bpy.data.images.load(scene.render.filepath, check_existing=False)
    render.colorspace_settings.name = 'Non-Color'
    pixels = np.empty(2048*2048*4, dtype=np.float32)
    render.pixels.foreach_get(pixels)
    height = pixels.reshape(2048,2048,4)[:,:,0] * spec.height*1.1
    bpy.data.images.remove(render)
    # Average the height before differentiating so isolated tips do not become sharp normal spikes.
    for _ in range(3):
        height = (height*4 + np.roll(height,1,0)+np.roll(height,-1,0)+np.roll(height,1,1)+np.roll(height,-1,1))/8
    dx = (np.roll(height,-1,1)-np.roll(height,1,1))/(size/2048*2)*.07
    dy = (np.roll(height,-1,0)-np.roll(height,1,0))/(size/2048*2)*.07
    normal = np.stack((-dx,dy,np.ones_like(dx)),axis=-1)
    normal /= np.linalg.norm(normal,axis=-1)[:,:,None]
    roughness = .96 - .04 * np.clip(height / spec.height, 0, 1)
    for role, data in (('NormalDX',normal*.5+.5), ('Roughness',np.repeat(roughness[:,:,None],3,axis=2))):
        image = bpy.data.images.new(f'T_Turf{family}_{role}', width=2048, height=2048, float_buffer=True)
        image.colorspace_settings.name = 'Non-Color'
        rgba = np.ones((2048,2048,4),dtype=np.float32)
        rgba[:,:,:3] = data
        image.pixels.foreach_set(rgba.ravel())
        png8.save(image, OUT/f'T_Turf{family}_{role}.png', False)
        bpy.data.images.remove(image)
    bpy.data.scenes.remove(scene)
    print('OPEN_GOLF_TURF_BAKED ' + family, flush=True)


if __name__ == '__main__':
    for name in SURFACES:
        bake(name)
    os._exit(0)
