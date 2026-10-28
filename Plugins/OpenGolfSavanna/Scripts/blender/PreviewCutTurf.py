# SPDX-License-Identifier: MIT
"""Render matching grass patches before and after a source revision."""
from pathlib import Path
import sys
import math
import bpy
from mathutils import Vector

HERE=Path(__file__).resolve().parent
sys.path.insert(0,str(HERE))
sys.path.insert(0,str(HERE.parents[2]/'OpenGolfCourse/Scripts'))
from turf_rules import SURFACES
from art_root import art_root


def linear_color(hex_color):
    rgb=[int(hex_color[i:i+2],16)/255 for i in (0,2,4)]
    return tuple(c/12.92 if c<=.04045 else ((c+.055)/1.055)**2.4 for c in rgb)


def blade_material(family):
    name='Preview Cut Turf '+family
    material=bpy.data.materials.get(name)
    if material:
        return material
    material=bpy.data.materials.new(name)
    material.use_nodes=True
    color=linear_color(SURFACES[family].color)
    material.diffuse_color=(*color,1)
    shader=material.node_tree.nodes.get('Principled BSDF')
    shader.inputs['Roughness'].default_value=.94
    shader.inputs['Normal'].default_value=(0,0,1)
    attribute=material.node_tree.nodes.new('ShaderNodeVertexColor')
    attribute.layer_name='BladeHeight'
    separate=material.node_tree.nodes.new('ShaderNodeSeparateColor')
    material.node_tree.links.new(attribute.outputs['Color'],separate.inputs['Color'])
    tone=material.node_tree.nodes.new('ShaderNodeMapRange')
    tone.inputs['To Min'].default_value=.82
    tone.inputs['To Max'].default_value=1.14
    material.node_tree.links.new(separate.outputs['Green'],tone.inputs['Value'])
    tint=material.node_tree.nodes.new('ShaderNodeVectorMath')
    tint.operation='SCALE'
    tint.inputs[0].default_value=color
    material.node_tree.links.new(tone.outputs['Result'],tint.inputs['Scale'])
    material.node_tree.links.new(tint.outputs['Vector'],shader.inputs['Base Color'])
    return material


def render_patch(meshes,family,label,output):
    scene=bpy.data.scenes.new('Turf preview '+label)
    scene.render.engine='CYCLES'
    scene.cycles.samples=24
    scene.cycles.use_denoising=True
    scene.render.resolution_x=1280
    scene.render.resolution_y=900
    scene.render.resolution_percentage=100
    scene.render.image_settings.file_format='PNG'
    scene.render.image_settings.color_depth='8'
    scene.view_settings.view_transform='Standard'
    scene.view_settings.look='None'
    world=bpy.data.worlds.new('Turf preview world')
    world.use_nodes=True
    world.node_tree.nodes['Background'].inputs['Color'].default_value=(.35,.42,.5,1)
    world.node_tree.nodes['Background'].inputs['Strength'].default_value=.7
    scene.world=world
    for index,(x,y) in enumerate(((-.25,-.25),(.25,-.25),(-.25,.25),(.25,.25))):
        mesh=meshes[index].copy()
        mesh.materials.clear()
        mesh.materials.append(blade_material(family))
        obj=bpy.data.objects.new('Turf patch',mesh)
        obj.location=(x,y,0)
        obj.rotation_euler.z=index*math.pi/2
        obj.visible_shadow=False
        scene.collection.objects.link(obj)
    ground=bpy.data.meshes.new('Preview turf backing')
    ground.from_pydata([(-.5,-.5,-.0002),(.5,-.5,-.0002),(.5,.5,-.0002),(-.5,.5,-.0002)],[],[(0,1,2,3)])
    mat=bpy.data.materials.new('Preview turf backing '+label)
    mat.diffuse_color=(*linear_color(SURFACES[family].color),1)
    mat.use_nodes=True
    shader=mat.node_tree.nodes['Principled BSDF']
    shader.inputs['Base Color'].default_value=tuple(c*.83 for c in mat.diffuse_color[:3])+(1,)
    shader.inputs['Roughness'].default_value=.94
    ground.materials.append(mat)
    scene.collection.objects.link(bpy.data.objects.new('Backing',ground))
    light=bpy.data.lights.new('Preview soft daylight','AREA')
    light.energy=450
    light.shape='DISK'
    light.size=4
    lamp=bpy.data.objects.new('Preview daylight',light)
    lamp.location=(-2,-3,5)
    lamp.rotation_euler=(-lamp.location).to_track_quat('-Z','Y').to_euler()
    scene.collection.objects.link(lamp)
    camera=bpy.data.cameras.new('Preview camera')
    camera.lens=55
    cam=bpy.data.objects.new('Preview camera',camera)
    cam.location=(1.1,-1.5,.65)
    cam.rotation_euler=(Vector((0,0,.01))-cam.location).to_track_quat('-Z','Y').to_euler()
    scene.collection.objects.link(cam)
    scene.camera=cam
    scene.render.filepath=str(output)
    output.parent.mkdir(parents=True,exist_ok=True)
    bpy.ops.render.render(scene=scene.name,write_still=True)
    bpy.data.scenes.remove(scene)
    print('CUT_TURF_PREVIEW_SAVED '+str(output),flush=True)


def preview(family='Fairway'):
    for obj in bpy.context.scene.objects:
        if obj.type=='MESH' and 'family' in obj:
            obj.data.materials.clear()
            obj.data.materials.append(blade_material(obj['family']))
    current=[bpy.data.objects[f'SM_Turf{family}_50cm_{v}'].evaluated_get(bpy.context.evaluated_depsgraph_get()) for v in 'ABCD']
    meshes=[bpy.data.meshes.new_from_object(obj) for obj in current]
    output=HERE.parents[3]/'Saved/Automation/CutTurf/Fullness'
    render_patch(meshes,family,'after',output/(family+'-after.png'))
    backup=art_root()/'CutTurf/CutTurf-before-fullness.blend'
    with bpy.data.libraries.load(str(backup),link=False) as (_,loaded):
        loaded.meshes=[f'SM_Turf{family}_50cm_{v}' for v in 'ABCD']
    if all(loaded.meshes):
        render_patch(loaded.meshes,family,'before',output/(family+'-before.png'))
    else:
        raise RuntimeError('The backup does not contain the original preview meshes')
    bpy.ops.wm.save_as_mainfile(filepath=str(art_root()/'CutTurf/CutTurf.blend'))
