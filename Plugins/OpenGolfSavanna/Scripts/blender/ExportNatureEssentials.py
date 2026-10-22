# SPDX-License-Identifier: MIT
"""Export the Nature Essentials (CC BY 4.0) pieces used by the savanna understory from an editable
Blender project.

    blender --background --python ExportNatureEssentials.py -- [--source PACK.blend] [--regenerate] [--art-root DIR]

The project, <art root>/NatureEssentials/NatureEssentials.blend, lives outside the repository
(see Plugins/OpenGolfCourse/Scripts/art_root.py). It is built from the purchased pack when it is
missing or with --regenerate (which discards edits); name the pack with --source or the
UOPENGOLF_NE_PACK environment variable. Its full-resolution textures are external
files in textures/ beside it. Edit it in Blender (variants, Budget decimation, litter pieces,
materials) and rerun to export: the file on disk is opened, never resaved. The pack's own .blend
is only read.

Outputs go to Saved/AssetImport/NatureEssentials: one FBX per mesh, PNG copies of every texture
a material uses (2K colour and normal maps, 1K masks), and export.json, which
ImportNatureEssentials.py reads in the editor.
"""
from pathlib import Path
import hashlib
import json
import math
import os
import sys

import bpy
import mathutils

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import ne_rules as rules

PROJECT = HERE.parents[3]
sys.path.insert(0, str(PROJECT / "Plugins/OpenGolfCourse/Scripts"))
from art_root import art_root

ART = art_root() / "NatureEssentials"
BLEND = ART / "NatureEssentials.blend"
TEXTURES = ART / "textures"
OUT = PROJECT / "Saved/AssetImport/NatureEssentials"
EXPORTED = {}
# The purchased pack's "(CG Geek) Nature Essentials Assets.blend" (installed by the GScatter add-on).
DEFAULT_SOURCE = Path(os.environ.get("UOPENGOLF_NE_PACK", "(CG Geek) Nature Essentials Assets.blend"))


def arguments():
    args = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    source = Path(args[args.index("--source") + 1]) if "--source" in args else DEFAULT_SOURCE
    return source, "--regenerate" in args


def child(parent, name):
    collection = bpy.data.collections.get(name) or bpy.data.collections.new(name)
    if collection.name not in parent.children:
        parent.children.link(collection)
    return collection


def move(obj, target):
    for collection in list(obj.users_collection):
        collection.objects.unlink(obj)
    target.objects.link(obj)


def triangles(obj):
    evaluated = obj.evaluated_get(bpy.context.evaluated_depsgraph_get())
    mesh = evaluated.to_mesh()
    mesh.calc_loop_triangles()
    count = len(mesh.loop_triangles)
    evaluated.to_mesh_clear()
    return count


def name_material(objects, name):
    """Every slot of objects uses one material called name, set on the mesh so linked duplicates
    agree; the first material found is renamed."""
    canonical = bpy.data.materials.get(name)
    for obj in objects:
        for slot in obj.material_slots:
            if canonical is None and slot.material:
                canonical = slot.material
                canonical.name = name
        if canonical is None:
            raise RuntimeError(f"{obj.name} has no material for {name}")
        for slot in obj.material_slots:
            slot.link = "DATA"
        for index in range(len(obj.data.materials)):
            obj.data.materials[index] = canonical


def base_at_origin(obj):
    """Moves the object so its bounds are centred on the origin in XY and rest on Z = 0."""
    obj.location = (0, 0, 0)
    bpy.context.view_layer.update()
    corners = [obj.matrix_world @ mathutils.Vector(corner) for corner in obj.bound_box]
    obj.location = (-(min(c.x for c in corners) + max(c.x for c in corners)) / 2,
                    -(min(c.y for c in corners) + max(c.y for c in corners)) / 2,
                    -min(c.z for c in corners))


def budget(obj, max_tris):
    ratio = rules.decimate_ratio(triangles(obj), max_tris)
    modifier = obj.modifiers.get("Budget")
    if ratio < 1 and not modifier:
        modifier = obj.modifiers.new("Budget", "DECIMATE")
    if modifier:
        modifier.ratio = ratio


def externalise_images():
    """Write packed images once each to textures/, then point the project at those files."""
    TEXTURES.mkdir(parents=True, exist_ok=True)
    by_hash, used_names = {}, {}
    for image in list(bpy.data.images):
        if not image.packed_file:
            continue
        data = image.packed_file.data
        digest = hashlib.sha1(data).hexdigest()
        if digest in by_hash:
            image.user_remap(by_hash[digest])
            continue
        filename = rules.texture_filename(image.name, image.file_format)
        if used_names.get(filename, digest) != digest:
            stem, dot, extension = filename.rpartition(".")
            filename = f"{stem}_{digest[:6]}.{extension}"
        used_names[filename] = digest
        (TEXTURES / filename).write_bytes(data)
        image.filepath = f"//textures/{filename}"
        image.unpack(method="REMOVE")
        image.name = filename
        by_hash[digest] = image
    bpy.data.orphans_purge(do_recursive=True)


def build(source):
    if not source.exists():
        raise RuntimeError(f"Nature Essentials pack not found: {source}")
    bpy.ops.wm.read_homefile(use_empty=True)
    bpy.context.preferences.filepaths.save_version = 0  # no .blend1 copies of the packed first save
    scene = bpy.context.scene
    wanted = {asset["collection"] for asset in rules.ASSETS}
    wanted |= {collection for collection, _ in rules.LITTER["sources"].values()}
    wanted |= set(rules.TURF_SOURCES)
    with bpy.data.libraries.load(str(source), link=False) as (available, loaded):
        missing = wanted - set(available.collections)
        if missing:
            raise RuntimeError(f"Pack collections missing: {sorted(missing)}")
        loaded.collections = sorted(wanted)
    sources = child(scene.collection, "Sources")
    for collection in loaded.collections:
        sources.children.link(collection)
    turf = child(scene.collection, "TurfSources")
    for name in rules.TURF_SOURCES:
        sources.children.unlink(bpy.data.collections[name])
        turf.children.link(bpy.data.collections[name])
    export = child(scene.collection, "Export")

    # Snapshot each source list before moving picks out of it, so picks keep their indices.
    listed = {name: sorted((o for o in bpy.data.collections[name].all_objects if o.type == "MESH"), key=lambda o: o.name)
              for name in wanted}
    for asset in rules.ASSETS:
        objects = listed[asset["collection"]]
        if asset["pick"] >= len(objects):
            raise RuntimeError(f"{asset['collection']} has {len(objects)} meshes; {asset['name']} wants #{asset['pick']}")
        obj = objects[asset["pick"]]
        move(obj, child(export, asset["name"]))
        base_at_origin(obj)
        name_material([obj], asset["material"])
        budget(obj, asset["max_tris"])

    pools = {}
    for kind, (collection, material) in rules.LITTER["sources"].items():
        pieces = listed[collection]
        name_material(pieces, material)
        pools[kind] = pieces
    for index, suffix in enumerate(rules.LITTER["variants"]):
        patch = child(export, f"SM_NE_Litter_{suffix}")
        for kind, variant, x, y, z, yaw, tilt in rules.litter_layout(index + 1, {k: len(v) for k, v in pools.items()}):
            piece = pools[kind][variant].copy()  # linked duplicate: shares the source mesh
            piece.location = (x, y, z)
            piece.rotation_euler = (tilt, 0.0, yaw)
            piece.parent = None
            patch.objects.link(piece)

    for layer in bpy.context.view_layer.layer_collection.children:
        if layer.name in {"Sources", "TurfSources"}:
            layer.exclude = True
    ART.mkdir(parents=True, exist_ok=True)
    bpy.ops.wm.save_as_mainfile(filepath=str(BLEND), compress=True, relative_remap=True, check_existing=False)
    externalise_images()
    bpy.ops.wm.save_as_mainfile(filepath=str(BLEND), compress=True, relative_remap=True, check_existing=False)
    print("NE_PROJECT_BUILT", BLEND)


def export_texture(source, role):
    """A copy of source for Unreal, no larger than 2K, as PNG; the full-size original stays in the project."""
    if source in EXPORTED:
        return EXPORTED[source]
    image = bpy.data.images.load(source, check_existing=False)
    image.colorspace_settings.name = "sRGB" if role in ("albedo", "opacity_alpha") else "Non-Color"
    size = rules.export_size(*image.size, rules.EXPORT_LIMITS[role])
    if tuple(image.size) != size:
        image.scale(*size)
    target = OUT / "textures" / (Path(source).stem + ".png")
    target.parent.mkdir(parents=True, exist_ok=True)
    image.filepath_raw = str(target)
    image.file_format = "PNG"
    image.save()
    bpy.data.images.remove(image)
    EXPORTED[source] = str(target)
    return EXPORTED[source]


def material_roles(material):
    """Unreal-ready texture copies by role, read from the material's node graph."""
    result = {}
    for node in material.node_tree.nodes:
        if node.bl_idname != "ShaderNodeTexImage" or not node.image:
            continue
        links = []
        for output in node.outputs:
            for link in output.links:
                target = link.to_node
                # A Mix with an unlinked zero factor ignores its B input (the pack's unused "old" grass colour).
                if (target.bl_idname == "ShaderNodeMix" and link.to_socket.name == "B" and not target.inputs[0].is_linked
                        and target.inputs[0].default_value == 0):
                    continue
                links.append(f"{output.name}->{target.bl_idname}:{target.name}.{link.to_socket.name}")
        for role in rules.image_roles(links):
            path = Path(bpy.path.abspath(node.image.filepath)).resolve()
            if not path.exists():
                raise RuntimeError(f"{material.name}: texture file missing: {path}")
            if role not in result:
                result[role] = export_texture(str(path), role)
                result.setdefault("sources", {})[role] = str(path)
    ramp = next((n for n in material.node_tree.nodes if n.bl_idname == "ShaderNodeValToRGB"), None)
    if ramp:
        elements = ramp.color_ramp.elements
        result["roughness_ramp"] = [[round(e.position, 4), round(e.color[0], 4)] for e in (elements[0], elements[-1])]
    for role in ("albedo",):
        if role not in result:
            raise RuntimeError(f"{material.name}: no {role} texture found")
    return result


def export():
    bpy.ops.wm.open_mainfile(filepath=str(BLEND))
    print("NE_PROJECT_OPENED", BLEND)
    meshes = OUT / "meshes"
    meshes.mkdir(parents=True, exist_ok=True)
    root = bpy.data.collections.get("Export")
    if not root:
        raise RuntimeError("The project has no Export collection")
    kinds = {asset["name"]: asset["kind"] for asset in rules.ASSETS}
    manifest = {"source_project": str(BLEND), "normal_convention": "OpenGL", "meshes": [], "materials": {}}
    depsgraph = bpy.context.evaluated_depsgraph_get()
    for collection in sorted(root.children, key=lambda c: c.name):
        objects = [o for o in collection.objects if o.type == "MESH"]
        if not objects:
            raise RuntimeError(f"{collection.name} has no meshes")
        bpy.ops.object.select_all(action="DESELECT")
        for obj in objects:
            obj.select_set(True)
        bpy.context.view_layer.objects.active = objects[0]
        # Shared meshes confuse the FBX material registry; give this session's copies their own data.
        # The project on disk keeps its linked duplicates because it is never resaved here.
        bpy.ops.object.make_single_user(object=False, obdata=True, material=False)
        fbx = meshes / f"{collection.name}.fbx"
        bpy.ops.export_scene.fbx(filepath=str(fbx), use_selection=True, object_types={"MESH"}, add_leaf_bones=False,
                                 axis_forward="-Y", axis_up="Z", bake_anim=False, mesh_smooth_type="FACE",
                                 use_mesh_modifiers=True)
        low, high = [math.inf] * 3, [-math.inf] * 3
        for obj in objects:
            evaluated = obj.evaluated_get(depsgraph)
            for corner in evaluated.bound_box:
                world = obj.matrix_world @ mathutils.Vector(corner)
                low = [min(a, b) for a, b in zip(low, world)]
                high = [max(a, b) for a, b in zip(high, world)]
        slots = sorted({slot.material.name for obj in objects for slot in obj.material_slots if slot.material})
        for name in slots:
            if name not in manifest["materials"]:
                manifest["materials"][name] = material_roles(bpy.data.materials[name])
        manifest["meshes"].append(dict(
            name=collection.name, kind=kinds.get(collection.name, "card"), fbx=str(fbx),
            triangles=sum(triangles(o) for o in objects), pieces=len(objects), materials=slots,
            bounds_m=[[round(v, 4) for v in low], [round(v, 4) for v in high]]))
        print("NE_EXPORTED", collection.name, manifest["meshes"][-1]["triangles"], "tris")
    (OUT / "export.json").write_text(json.dumps(manifest, indent=2))
    print("NE_EXPORT_COMPLETE", len(manifest["meshes"]), "meshes", len(manifest["materials"]), "materials")


def main():
    source, regenerate = arguments()
    if regenerate or not BLEND.exists():
        build(source)
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
