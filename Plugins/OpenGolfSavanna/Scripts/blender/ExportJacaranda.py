# SPDX-License-Identifier: MIT
"""Export the Poly Haven jacaranda (CC0) from an editable Blender project.

    python3 Plugins/OpenGolfSavanna/Scripts/FetchPolyHaven.py jacaranda_tree
    blender --background --python Plugins/OpenGolfSavanna/Scripts/blender/ExportJacaranda.py -- [--regenerate] [--art-root DIR]

The project, <art root>/Jacaranda/Jacaranda.blend, lives outside the repository. It is built
from Poly Haven's geometry-node tree when it is missing or with --regenerate (which discards
edits) and is opened, never resaved, for every export. Settings worth editing in Blender:
the `Budget` decimate on the branches, each leaf modifier's Value (leaf density) and random
size, and the trunk object used by the node group. Its textures stay in the downloaded
PolyHaven/jacaranda_tree/textures folder.

Outputs go to Saved/AssetImport/OpenGolfSavanna/Jacaranda: SM_Jacaranda.fbx, the hidden ball-contact
helpers SM_JacarandaCollision.fbx (trunk and main limbs) and SM_JacarandaCanopy.fbx (one convex hull
around the branches and leaves, with its UCX_ collision), texture copies
(2K colour and normal maps, 1K masks) and export.json for ImportSavannaAssets.py.
"""
from pathlib import Path
import json
import math
import os
import sys

import bmesh
import bpy
import mathutils

HERE = Path(__file__).resolve().parent
PROJECT = HERE.parents[3]
sys.path.insert(0, str(HERE))
sys.path.insert(0, str(PROJECT / "Plugins/OpenGolfCourse/Scripts"))
import export_rules as rules
import png8
from art_root import art_root

ROOT = art_root()
SOURCE = ROOT / "PolyHaven/jacaranda_tree/jacaranda_tree_2k.blend"
BLEND = ROOT / "Jacaranda/Jacaranda.blend"
OUT = PROJECT / "Saved/AssetImport/OpenGolfSavanna/Jacaranda"
TREE = "jacaranda_tree_geometry_nodes"
MESH_NAME = "SM_Jacaranda"
# Starting look, chosen to keep the asset light enough to commit (about 0.7M triangles instead of
# 3.9M): fewer, slightly larger leaf clusters, the lighter trunk, and half the branch triangles.
LEAF_VALUES = {"jacaranda_tree_leaves_main": 15.0, "jacaranda_tree_leaves_tips": 100.0}
LEAF_SIZE_SCALE = 1.25
BRANCH_RATIO = 0.5
CARD_MATERIALS = {"jacaranda_tree_leaves"}
# Hidden helpers for ball contact: the trunk and main limbs stop the ball; branches and leaves
# only slow it, inside one convex canopy volume (Unreal's simple collision allows few hull points).
COLLISION_NAME = "SM_JacarandaCollision"
WOOD_MATERIALS = {"jacaranda_tree_trunk"}
CANOPY_NAME = "SM_JacarandaCanopy"
CANOPY_MATERIALS = {"jacaranda_tree_branches", "jacaranda_tree_leaves"}
CANOPY_POINTS = 64
EXPORTED = {}


def inputs(modifier):
    return {item.name: item.identifier for item in modifier.node_group.interface.items_tree
            if getattr(item, "in_out", "") == "INPUT"}


def exclude(layer, name):
    for child in layer.children:
        if child.name == name:
            child.exclude = True
        exclude(child, name)


def evaluated_stats(tree):
    """Triangles per material, world bounds, trunk base and canopy radius of the evaluated tree."""
    depsgraph = bpy.context.evaluated_depsgraph_get()
    evaluated = tree.evaluated_get(depsgraph)
    mesh = evaluated.to_mesh()
    mesh.calc_loop_triangles()
    per_material, trunk = {}, set()
    for triangle in mesh.loop_triangles:
        name = mesh.materials[triangle.material_index].name
        per_material[name] = per_material.get(name, 0) + 1
        if name == "jacaranda_tree_trunk":
            trunk.update(triangle.vertices)
    points = [tree.matrix_world @ v.co for v in mesh.vertices]
    trunk_points = [points[i] for i in trunk]
    evaluated.to_mesh_clear()
    base = mathutils.Vector((sum(p.x for p in trunk_points) / len(trunk_points),
                             sum(p.y for p in trunk_points) / len(trunk_points), min(p.z for p in trunk_points)))
    low = [min(p[i] for p in points) for i in range(3)]
    high = [max(p[i] for p in points) for i in range(3)]
    radius = max(math.hypot(p.x - base.x, p.y - base.y) for p in points)
    return per_material, low, high, base, radius


def build():
    if not SOURCE.exists():
        raise RuntimeError(f"Run FetchPolyHaven.py first: {SOURCE} is missing")
    bpy.ops.wm.read_homefile(use_empty=True)
    bpy.context.preferences.filepaths.save_version = 0
    with bpy.data.libraries.load(str(SOURCE), link=False) as (available, loaded):
        loaded.collections = [TREE]
    bpy.context.scene.collection.children.link(loaded.collections[0])
    exclude(bpy.context.view_layer.layer_collection, "jacaranda_tree_geometry")
    tree = bpy.data.objects[TREE]
    main, tips = tree.modifiers["jacaranda_tree_leaves_main"], tree.modifiers["jacaranda_tree_leaves_tips"]
    # Both modifiers share one node group that also joins the trunk, which doubled it; the tips get
    # their own copy without the trunk.
    group = main.node_group
    group_info = next(n for n in group.nodes if n.bl_idname == "GeometryNodeObjectInfo")
    group_info.inputs["Object"].default_value = bpy.data.objects["jacaranda_tree_trunk_LOD1"]
    tips.node_group = group.copy()
    tips.node_group.name = "jacaranda_tree_leaves_tips"
    next(n for n in tips.node_group.nodes if n.bl_idname == "GeometryNodeObjectInfo").inputs["Object"].default_value = None
    for modifier in (main, tips):
        ids = inputs(modifier)
        modifier[ids["Value"]] = LEAF_VALUES[modifier.name]
        modifier[ids["random size min"]] = modifier[ids["random size min"]] * LEAF_SIZE_SCALE
        modifier[ids["random size max"]] = modifier[ids["random size max"]] * LEAF_SIZE_SCALE
    budget = tree.modifiers.new("Budget", "DECIMATE")
    budget.ratio = BRANCH_RATIO
    tree.modifiers.move(len(tree.modifiers) - 1, 0)
    # Poly Haven places this variant beside its baked LODs; stand the trunk base on the origin.
    tree.location -= evaluated_stats(tree)[3]
    bpy.context.view_layer.update()
    BLEND.parent.mkdir(parents=True, exist_ok=True)
    bpy.ops.wm.save_as_mainfile(filepath=str(BLEND), compress=True, relative_remap=True, check_existing=False)
    print("JACARANDA_PROJECT_BUILT", BLEND)


def export_texture(source, role):
    if source in EXPORTED:
        return EXPORTED[source]
    image = png8.load(source, rules.is_colour(role))
    size = rules.export_size(*image.size, rules.EXPORT_LIMITS[role])
    if tuple(image.size) != size:
        image.scale(*size)
    target = OUT / "textures" / (Path(source).stem + ".png")
    target.parent.mkdir(parents=True, exist_ok=True)
    # Poly Haven ships 16-bit PNGs; Image.save() would keep 16 bits (see png8).
    png8.save(image, target, rules.is_colour(role))
    bpy.data.images.remove(image)
    EXPORTED[source] = str(target)
    return EXPORTED[source]


def _shader_of(link):
    """The BSDF or group node a texture link ends in, looking through Normal Map nodes."""
    node = link.to_node
    if node.bl_idname == "ShaderNodeNormalMap":
        links = node.outputs["Normal"].links
        return links[0].to_node if links else None
    return node


def _layer(textures, material):
    if "albedo" not in textures:
        raise RuntimeError(f"{material.name}: a layer has no colour texture")
    return {role: export_texture(path, role) for role, path in textures.items()}


def material_roles(material):
    """Texture layers for Unreal. Poly Haven's trunk mixes two bark layers by the vertex colour's red
    channel (0 = first layer, 1 = second); other materials have one layer."""
    by_shader = {}
    for node in material.node_tree.nodes:
        if node.bl_idname != "ShaderNodeTexImage" or not node.image:
            continue
        path = Path(bpy.path.abspath(node.image.filepath)).resolve()
        if not path.exists():
            raise RuntimeError(f"{material.name}: texture file missing: {path}")
        for output in node.outputs:
            for link in output.links:
                shader = _shader_of(link)
                for role in rules.principled_roles([f"{output.name}->{link.to_node.bl_idname}.{link.to_socket.name}"]):
                    by_shader.setdefault(shader.name, {})[role] = str(path)
    result = next(n for n in material.node_tree.nodes if n.bl_idname == "ShaderNodeOutputMaterial" and n.is_active_output)
    surface = result.inputs["Surface"].links[0].from_node
    blend = None
    if surface.bl_idname == "ShaderNodeMixShader":
        factor = surface.inputs[0].links[0].from_node
        source = factor.inputs[0].links[0].from_node if factor.inputs[0].links else None
        if factor.bl_idname != "ShaderNodeSeparateColor" or not source or source.bl_idname != "ShaderNodeVertexColor":
            raise RuntimeError(f"{material.name}: unsupported mix factor {factor.bl_idname}")
        shaders, blend = [surface.inputs[1].links[0].from_node, surface.inputs[2].links[0].from_node], "vertex_red"
    else:
        shaders = [surface]
    layers = [_layer(by_shader.get(shader.name, {}), material) for shader in shaders]
    return {"kind": "card" if material.name in CARD_MATERIALS else "solid", "layers": layers, "blend": blend,
            "sources": [by_shader.get(shader.name, {}) for shader in shaders]}


def export_fbx(path, objects, modifiers):
    bpy.ops.object.select_all(action="DESELECT")
    for obj in objects:
        obj.select_set(True)
    bpy.context.view_layer.objects.active = objects[0]
    bpy.ops.export_scene.fbx(filepath=str(path), use_selection=True, object_types={"MESH"}, add_leaf_bones=False,
                             axis_forward="-Y", axis_up="Z", bake_anim=False, mesh_smooth_type="FACE",
                             use_mesh_modifiers=modifiers)


def helper_object(name, mesh, tree, material):
    """An object at the tree's transform, so its export lines up with SM_Jacaranda."""
    mesh.materials.clear()
    mesh.materials.append(material)
    for polygon in mesh.polygons:
        polygon.material_index = 0
    obj = bpy.data.objects.new(name, mesh)
    bpy.context.scene.collection.objects.link(obj)
    obj.matrix_world = tree.matrix_world.copy()
    return obj


def evaluated_copy(tree):
    return bpy.data.meshes.new_from_object(tree.evaluated_get(bpy.context.evaluated_depsgraph_get()))


def collision_object(tree):
    """The tree's wood faces only, as a separate mesh."""
    mesh = evaluated_copy(tree)
    names = [mesh.materials[p.material_index].name for p in mesh.polygons]
    keep = set(rules.faces_with_materials(names, WOOD_MATERIALS))
    if not keep:
        raise RuntimeError(f"No faces use {sorted(WOOD_MATERIALS)}")
    bm = bmesh.new()
    bm.from_mesh(mesh)
    bm.faces.ensure_lookup_table()
    bmesh.ops.delete(bm, geom=[f for f in bm.faces if f.index not in keep], context="FACES")
    bmesh.ops.delete(bm, geom=[v for v in bm.verts if not v.link_faces], context="VERTS")
    bm.to_mesh(mesh)
    bm.free()
    return helper_object(COLLISION_NAME, mesh, tree, bpy.data.materials[next(iter(WOOD_MATERIALS))])


def _hull(points):
    bm = bmesh.new()
    verts = [bm.verts.new(p) for p in points]
    result = bmesh.ops.convex_hull(bm, input=verts)
    loose = {v for v in result["geom_interior"] + result["geom_unused"] if isinstance(v, bmesh.types.BMVert)}
    bmesh.ops.delete(bm, geom=list(loose), context="VERTS")
    return bm


def canopy_objects(tree):
    """One convex hull around the branches and leaves, as a render mesh and its UCX_ collision."""
    mesh = evaluated_copy(tree)
    used = {v for p in mesh.polygons if mesh.materials[p.material_index].name in CANOPY_MATERIALS for v in p.vertices}
    bm = _hull([mesh.vertices[i].co for i in used])
    corners = rules.farthest_points([tuple(v.co) for v in bm.verts], CANOPY_POINTS)
    bm.free()
    bpy.data.meshes.remove(mesh)
    material = bpy.data.materials[next(iter(WOOD_MATERIALS))]
    objects = []
    for name in (CANOPY_NAME, f"UCX_{CANOPY_NAME}_00"):
        bm = _hull(corners)
        data = bpy.data.meshes.new(name)
        bm.to_mesh(data)
        bm.free()
        objects.append(helper_object(name, data, tree, material))
    return objects


def helper_entry(name, fbx, obj, collision, slots):
    low = [min((obj.matrix_world @ v.co)[i] for v in obj.data.vertices) for i in range(3)]
    high = [max((obj.matrix_world @ v.co)[i] for v in obj.data.vertices) for i in range(3)]
    return dict(name=name, kind=f"tree_{collision}", fbx=str(fbx), materials=sorted(WOOD_MATERIALS), nanite=False,
                collision=collision, slots=sorted(slots), triangles=sum(len(p.vertices) - 2 for p in obj.data.polygons),
                vertices=len(obj.data.vertices),
                bounds_m=[[round(v, 3) for v in low], [round(v, 3) for v in high]])


def export():
    bpy.ops.wm.open_mainfile(filepath=str(BLEND))
    print("JACARANDA_PROJECT_OPENED", BLEND)
    tree = bpy.data.objects[TREE]
    OUT.mkdir(parents=True, exist_ok=True)
    fbx = OUT / f"{MESH_NAME}.fbx"
    export_fbx(fbx, [tree], True)
    # The helpers are built in memory after opening the project, which is never resaved.
    collision = collision_object(tree)
    collision_fbx = OUT / f"{COLLISION_NAME}.fbx"
    export_fbx(collision_fbx, [collision], False)
    canopy = canopy_objects(tree)
    canopy_fbx = OUT / f"{CANOPY_NAME}.fbx"
    export_fbx(canopy_fbx, canopy, False)
    per_material, low, high, base, radius = evaluated_stats(tree)
    materials = sorted(per_material)
    manifest = {
        "source_project": str(BLEND), "source": str(SOURCE), "licence": "CC0 1.0 (Poly Haven)",
        "normal_convention": "OpenGL", "folder": "Trees/Jacaranda",
        # Helpers first: importing the tree refreshes its wood proxy from the imported collision mesh.
        "meshes": [helper_entry(COLLISION_NAME, collision_fbx, collision, "complex", WOOD_MATERIALS),
                   helper_entry(CANOPY_NAME, canopy_fbx, canopy[0], "convex", CANOPY_MATERIALS),
                   dict(name=MESH_NAME, kind="tree", fbx=str(fbx), triangles=sum(per_material.values()),
                        triangles_by_material=per_material, materials=materials,
                        bounds_m=[[round(v, 3) for v in low], [round(v, 3) for v in high]],
                        trunk_base_m=[round(v, 3) for v in base], canopy_radius_m=round(radius, 3))],
        "materials": {name: material_roles(bpy.data.materials[name]) for name in materials},
    }
    (OUT / "export.json").write_text(json.dumps(manifest, indent=2))
    tree_entry = manifest["meshes"][-1]
    print("JACARANDA_EXPORT_COMPLETE", tree_entry["triangles"], "tris", per_material,
          "collision", manifest["meshes"][0]["triangles"], "tris; canopy", manifest["meshes"][1]["vertices"], "points")


def main():
    args = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    if "--regenerate" in args or not BLEND.exists():
        build()
    export()


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
