# SPDX-License-Identifier: MIT
"""Export a geometry-node tree from an editable Blender project for ImportSavannaAssets.py (Blender only).

A species script (ExportJacaranda.py, the course plugins' tree exporters) describes its tree with a TreeSpec
and supplies the build() that makes its project from the Poly Haven source; run() then opens that project,
never resaving it, and writes to spec.out:

- <mesh>.fbx, the visible tree
- <mesh>_Collision.fbx, the hidden wood helper: the trunk and main limbs, which stop the ball
- <mesh>_Canopy.fbx, the hidden canopy helper: one convex hull around the branches and leaves, with its UCX_
  collision, which slows the ball
- 8-bit texture copies (2K colour and normal maps, 1K masks) and export.json for ImportSavannaAssets.py

    blender --background --python <species script> -- [--regenerate] [--out DIR] [--art-root DIR]
"""
from dataclasses import dataclass, field, replace
from pathlib import Path
import json
import math
import os
import sys

import bmesh
import bpy
import mathutils

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import export_rules as rules
import png8


@dataclass(frozen=True)
class TreeSpec:
    label: str  # log prefix, for example JACARANDA
    source: Path  # Poly Haven .blend
    blend: Path  # editable project in the Blender art root
    out: Path
    tree: str  # the geometry-node object
    mesh_name: str
    collision_name: str
    canopy_name: str
    trunk_material: str  # its lowest point and centre stand the tree on the origin
    wood_materials: frozenset
    canopy_materials: frozenset
    card_materials: frozenset
    folder: str  # content folder under the destination mount
    # Optional {"mount", "content_dir", "owner"} for content outside /OpenGolfSavanna.
    destination: dict = field(default_factory=dict)
    canopy_points: int = 64  # Unreal's simple collision allows few hull points


def inputs(modifier):
    """Geometry-node input identifiers by name."""
    return {item.name: item.identifier for item in modifier.node_group.interface.items_tree
            if getattr(item, "in_out", "") == "INPUT"}


def exclude(layer, name):
    for child in layer.children:
        if child.name == name:
            child.exclude = True
        exclude(child, name)


def load_tree(spec, collection, geometry_collection):
    """A fresh scene holding the source's geometry-node collection, with its source pieces hidden."""
    if not spec.source.exists():
        raise RuntimeError(f"Run FetchPolyHaven.py first: {spec.source} is missing")
    bpy.ops.wm.read_homefile(use_empty=True)
    bpy.context.preferences.filepaths.save_version = 0
    with bpy.data.libraries.load(str(spec.source), link=False) as (available, loaded):
        loaded.collections = [collection]
    bpy.context.scene.collection.children.link(loaded.collections[0])
    exclude(bpy.context.view_layer.layer_collection, geometry_collection)
    return bpy.data.objects[spec.tree]


def save_project(spec):
    spec.blend.parent.mkdir(parents=True, exist_ok=True)
    bpy.ops.wm.save_as_mainfile(filepath=str(spec.blend), compress=True, relative_remap=True, check_existing=False)
    print(f"{spec.label}_PROJECT_BUILT", spec.blend)


def evaluated_stats(spec, tree):
    """Triangles per material, world bounds, trunk base and canopy radius of the evaluated tree."""
    depsgraph = bpy.context.evaluated_depsgraph_get()
    evaluated = tree.evaluated_get(depsgraph)
    mesh = evaluated.to_mesh()
    mesh.calc_loop_triangles()
    per_material, trunk = {}, set()
    for triangle in mesh.loop_triangles:
        name = mesh.materials[triangle.material_index].name
        per_material[name] = per_material.get(name, 0) + 1
        if name == spec.trunk_material:
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


def stand_on_origin(spec, tree):
    """Move the tree so its trunk base sits on the origin."""
    tree.location -= evaluated_stats(spec, tree)[3]
    bpy.context.view_layer.update()


class _Textures:
    """8-bit copies of each source texture, written once per export."""

    def __init__(self, out):
        self.out = out
        self.exported = {}

    def export(self, source, role):
        if source in self.exported:
            return self.exported[source]
        image = png8.load(source, rules.is_colour(role))
        size = rules.export_size(*image.size, rules.EXPORT_LIMITS[role])
        if tuple(image.size) != size:
            image.scale(*size)
        target = self.out / "textures" / (Path(source).stem + ".png")
        target.parent.mkdir(parents=True, exist_ok=True)
        # Poly Haven ships 16-bit PNGs; Image.save() would keep 16 bits (see png8).
        png8.save(image, target, rules.is_colour(role))
        bpy.data.images.remove(image)
        self.exported[source] = str(target)
        return self.exported[source]


def _shader_of(link):
    """The BSDF or group node a texture link ends in, looking through Normal Map nodes."""
    node = link.to_node
    if node.bl_idname == "ShaderNodeNormalMap":
        links = node.outputs["Normal"].links
        return links[0].to_node if links else None
    return node


def _layer(textures, material, exporter):
    if "albedo" not in textures:
        raise RuntimeError(f"{material.name}: a layer has no colour texture")
    return {role: exporter.export(path, role) for role, path in textures.items()}


def material_roles(spec, material, exporter):
    """Texture layers for Unreal. Poly Haven's jacaranda trunk mixes two bark layers by the vertex colour's red
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
    layers = [_layer(by_shader.get(shader.name, {}), material, exporter) for shader in shaders]
    return {"kind": "card" if material.name in spec.card_materials else "solid", "layers": layers, "blend": blend,
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
    """An object at the tree's transform, so its export lines up with the visible tree."""
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


def collision_object(spec, tree):
    """The tree's wood faces only, as a separate mesh."""
    mesh = evaluated_copy(tree)
    names = [mesh.materials[p.material_index].name for p in mesh.polygons]
    keep = set(rules.faces_with_materials(names, spec.wood_materials))
    if not keep:
        raise RuntimeError(f"No faces use {sorted(spec.wood_materials)}")
    bm = bmesh.new()
    bm.from_mesh(mesh)
    bm.faces.ensure_lookup_table()
    bmesh.ops.delete(bm, geom=[f for f in bm.faces if f.index not in keep], context="FACES")
    bmesh.ops.delete(bm, geom=[v for v in bm.verts if not v.link_faces], context="VERTS")
    bm.to_mesh(mesh)
    bm.free()
    return helper_object(spec.collision_name, mesh, tree, bpy.data.materials[next(iter(spec.wood_materials))])


def _hull(points):
    bm = bmesh.new()
    verts = [bm.verts.new(p) for p in points]
    result = bmesh.ops.convex_hull(bm, input=verts)
    loose = {v for v in result["geom_interior"] + result["geom_unused"] if isinstance(v, bmesh.types.BMVert)}
    bmesh.ops.delete(bm, geom=list(loose), context="VERTS")
    return bm


def canopy_objects(spec, tree):
    """One convex hull around the branches and leaves, as a render mesh and its UCX_ collision."""
    mesh = evaluated_copy(tree)
    used = {v for p in mesh.polygons if mesh.materials[p.material_index].name in spec.canopy_materials for v in p.vertices}
    bm = _hull([mesh.vertices[i].co for i in used])
    corners = rules.farthest_points([tuple(v.co) for v in bm.verts], spec.canopy_points)
    bm.free()
    bpy.data.meshes.remove(mesh)
    material = bpy.data.materials[next(iter(spec.wood_materials))]
    objects = []
    for name in (spec.canopy_name, f"UCX_{spec.canopy_name}_00"):
        bm = _hull(corners)
        data = bpy.data.meshes.new(name)
        bm.to_mesh(data)
        bm.free()
        objects.append(helper_object(name, data, tree, material))
    return objects


def helper_entry(spec, name, fbx, obj, collision, slots):
    low = [min((obj.matrix_world @ v.co)[i] for v in obj.data.vertices) for i in range(3)]
    high = [max((obj.matrix_world @ v.co)[i] for v in obj.data.vertices) for i in range(3)]
    return dict(name=name, kind=f"tree_{collision}", fbx=str(fbx), materials=sorted(spec.wood_materials), nanite=False,
                collision=collision, slots=sorted(slots), triangles=sum(len(p.vertices) - 2 for p in obj.data.polygons),
                vertices=len(obj.data.vertices),
                bounds_m=[[round(v, 3) for v in low], [round(v, 3) for v in high]])


def export(spec, material_settings=None):
    """Write the meshes, textures and export.json. material_settings({name: roles}) returns extra instance
    settings per material name, for example a Tint, which ImportSavannaAssets.py applies."""
    bpy.ops.wm.open_mainfile(filepath=str(spec.blend))
    print(f"{spec.label}_PROJECT_OPENED", spec.blend)
    tree = bpy.data.objects[spec.tree]
    spec.out.mkdir(parents=True, exist_ok=True)
    fbx = spec.out / f"{spec.mesh_name}.fbx"
    export_fbx(fbx, [tree], True)
    # The helpers are built in memory after opening the project, which is never resaved.
    collision = collision_object(spec, tree)
    collision_fbx = spec.out / f"{spec.collision_name}.fbx"
    export_fbx(collision_fbx, [collision], False)
    canopy = canopy_objects(spec, tree)
    canopy_fbx = spec.out / f"{spec.canopy_name}.fbx"
    export_fbx(canopy_fbx, canopy, False)
    per_material, low, high, base, radius = evaluated_stats(spec, tree)
    materials = sorted(per_material)
    textures = _Textures(spec.out)
    manifest = {
        "source_project": str(spec.blend), "source": str(spec.source), "licence": "CC0 1.0 (Poly Haven)",
        "normal_convention": "OpenGL", "folder": spec.folder, **spec.destination,
        # Helpers first: importing the tree refreshes its wood proxy from the imported collision mesh.
        "meshes": [helper_entry(spec, spec.collision_name, collision_fbx, collision, "complex", spec.wood_materials),
                   helper_entry(spec, spec.canopy_name, canopy_fbx, canopy[0], "convex", spec.canopy_materials),
                   dict(name=spec.mesh_name, kind="tree", fbx=str(fbx), triangles=sum(per_material.values()),
                        triangles_by_material=per_material, materials=materials,
                        bounds_m=[[round(v, 3) for v in low], [round(v, 3) for v in high]],
                        trunk_base_m=[round(v, 3) for v in base], canopy_radius_m=round(radius, 3))],
        "materials": {name: material_roles(spec, bpy.data.materials[name], textures) for name in materials},
    }
    for name, settings in (material_settings(manifest["materials"]) if material_settings else {}).items():
        manifest["materials"][name]["settings"] = settings
    (spec.out / "export.json").write_text(json.dumps(manifest, indent=2))
    tree_entry = manifest["meshes"][-1]
    print(f"{spec.label}_EXPORT_COMPLETE", tree_entry["triangles"], "tris", per_material,
          "collision", manifest["meshes"][0]["triangles"], "tris; canopy", manifest["meshes"][1]["vertices"], "points")
    return manifest


def run(spec, build, material_settings=None):
    """Command-line entry: build the project when missing or with --regenerate, then export (to --out if given)."""
    args = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    if "--out" in args:
        spec = replace(spec, out=Path(args[args.index("--out") + 1]).resolve())
    try:
        if "--regenerate" in args or not spec.blend.exists():
            build()
        export(spec, material_settings)
    except Exception:
        import traceback
        traceback.print_exc()
        sys.stdout.flush()
        os._exit(1)
    sys.stdout.flush()
    os._exit(0)
