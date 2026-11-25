# SPDX-License-Identifier: MIT
"""Render look-check previews of an exported tree project, with the Tint its manifest gives each material.

    blender --background --python Plugins/OpenGolfSavanna/Scripts/blender/PreviewTree.py -- <export.json> <out dir>

Opens the manifest's source project (never resaving it), multiplies each material's colour texture by its Tint and
blends it toward its own grey by its Saturation, as the Unreal card material does (savanna_assets.card_material), and renders a front and a side view in daylight over dry golden grass, with a
1.8 m figure for scale, to <out dir>/preview-front.png and preview-side.png. These are for judging size, shape
and colour against references; they are not Unreal captures and say nothing about shimmer or cost.
"""
from pathlib import Path
import json
import math
import os
import sys

import bpy
import mathutils

FIGURE_M = 1.8
VIEWS = {"front": (0.0, -1.0), "side": (1.0, 0.0)}
# Linear colours: a clear sky and summer-dry California grass.
SKY = (0.32, 0.48, 0.78)
GRASS = (0.52, 0.36, 0.12)


def tint_material(material, tint, saturation=1.0):
    """Multiply the material's colour texture by tint, then blend toward its grey by saturation, between the texture
    and its shader input."""
    tree = material.node_tree
    for node in tree.nodes:
        if node.bl_idname != "ShaderNodeTexImage" or not node.image:
            continue
        for link in list(node.outputs["Color"].links):
            if link.to_socket.name not in ("Base Color", "Diffuse"):
                continue
            mix = tree.nodes.new("ShaderNodeMix")
            mix.data_type = "RGBA"
            mix.blend_type = "MULTIPLY"
            mix.inputs["Factor"].default_value = 1.0
            mix.inputs["B"].default_value = (*tint[:3], 1.0)
            grey = tree.nodes.new("ShaderNodeRGBToBW")
            blend = tree.nodes.new("ShaderNodeMix")
            blend.data_type = "RGBA"
            blend.inputs["Factor"].default_value = saturation
            target = link.to_socket
            tree.links.remove(link)
            tree.links.new(node.outputs["Color"], mix.inputs["A"])
            tree.links.new(mix.outputs["Result"], grey.inputs["Color"])
            tree.links.new(grey.outputs["Val"], blend.inputs["A"])
            tree.links.new(mix.outputs["Result"], blend.inputs["B"])
            tree.links.new(blend.outputs["Result"], target)
            return True
    raise RuntimeError(f"{material.name}: no colour texture to tint")


def flat_material(name, colour):
    material = bpy.data.materials.new(name)
    material.use_nodes = True
    shader = next(n for n in material.node_tree.nodes if n.type == "BSDF_PRINCIPLED")
    shader.inputs["Base Color"].default_value = (*colour, 1.0)
    shader.inputs["Roughness"].default_value = 0.9
    return material


def set_engine(scene):
    names = [item.identifier for item in bpy.types.RenderSettings.bl_rna.properties["engine"].enum_items]
    for engine in ("BLENDER_EEVEE", "BLENDER_EEVEE_NEXT", "CYCLES"):
        if engine in names:
            scene.render.engine = engine
            return engine
    raise RuntimeError(f"No usable render engine in {names}")


def stage(tree_height, tree_width):
    scene = bpy.context.scene
    engine = set_engine(scene)
    if engine == "CYCLES":
        scene.cycles.samples = 64
    scene.render.resolution_x, scene.render.resolution_y = 1280, 960
    scene.view_settings.view_transform = "Standard"
    world = scene.world or bpy.data.worlds.new("Preview")
    scene.world = world
    world.use_nodes = True
    background = next(n for n in world.node_tree.nodes if n.type == "BACKGROUND")
    background.inputs["Color"].default_value = (*SKY, 1.0)
    background.inputs["Strength"].default_value = 1.0
    sun = bpy.data.objects.new("Sun", bpy.data.lights.new("Sun", "SUN"))
    sun.data.energy = 4.0
    sun.data.angle = math.radians(1.0)
    sun.rotation_euler = (math.radians(40), 0.0, math.radians(-35))
    scene.collection.objects.link(sun)
    bpy.ops.mesh.primitive_plane_add(size=max(80.0, tree_width * 6))
    bpy.context.object.data.materials.append(flat_material("Grass", GRASS))
    bpy.ops.mesh.primitive_cylinder_add(radius=0.22, depth=FIGURE_M, location=(tree_width * 0.6, -tree_width * 0.3, FIGURE_M / 2))
    bpy.context.object.data.materials.append(flat_material("Figure", (0.08, 0.08, 0.08)))
    camera = bpy.data.objects.new("Camera", bpy.data.cameras.new("Camera"))
    camera.data.lens = 35
    scene.collection.objects.link(camera)
    scene.camera = camera
    return scene, camera


def main():
    manifest_path, out = sys.argv[sys.argv.index("--") + 1:][:2]
    manifest = json.loads(Path(manifest_path).read_text())
    out = Path(out)
    out.mkdir(parents=True, exist_ok=True)
    bpy.ops.wm.open_mainfile(filepath=manifest["source_project"])
    for name, entry in manifest["materials"].items():
        settings = entry.get("settings", {})
        if "Tint" in settings or "Saturation" in settings:
            tint_material(bpy.data.materials[name], settings.get("Tint", (1.0, 1.0, 1.0)), settings.get("Saturation", 1.0))
    low, high = next(m for m in manifest["meshes"] if m["kind"] == "tree")["bounds_m"]
    height, width = high[2] - low[2], max(high[0] - low[0], high[1] - low[1])
    scene, camera = stage(height, width)
    target = mathutils.Vector((0.0, 0.0, height * 0.45))
    distance = max(height, width) * 1.9
    for view, (x, y) in VIEWS.items():
        camera.location = (x * distance, y * distance, 1.7)
        camera.rotation_euler = (target - camera.location).to_track_quat("-Z", "Y").to_euler()
        scene.render.filepath = str(out / f"preview-{view}.png")
        bpy.ops.render.render(write_still=True)
        print("TREE_PREVIEW", view, scene.render.filepath)


if __name__ == "__main__":
    try:
        main()
    except Exception:
        import traceback
        traceback.print_exc()
        sys.stdout.flush()
        os._exit(1)
    sys.stdout.flush()
    os._exit(0)
