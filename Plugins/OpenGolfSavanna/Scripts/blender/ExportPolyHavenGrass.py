# SPDX-License-Identifier: MIT
"""Export dense Poly Haven (CC0) grass tiles from an editable Blender project.

    python3 Plugins/OpenGolfSavanna/Scripts/FetchPolyHaven.py grass_medium_01 leafy_grass
    blender --background --python Plugins/OpenGolfSavanna/Scripts/blender/ExportPolyHavenGrass.py -- [--regenerate] [--art-root DIR]

The project, <art root>/Grass/Grass.blend, lives outside the repository. It is built when it is
missing or with --regenerate (which discards edits) and is opened, never resaved, for every
export. Its `Tiles` collection holds one tile per Unreal mesh (grass_rules.TILES):
  SM_GrassMedium_30cm_{1m,50cm,25cm}_*  squares of Grass Medium's tufts (the `Pieces` collection,
                                        LOD0-2), one per seeded 1/16 m grid cell
  SM_GrassMedium_30cm_Edge_*            1 x 0.25 m strips of shorter tufts for play edges
Edit a tile mesh directly. Laid edge to edge, the squares give full cover; the course kit's tiled
placement subdivides them near outlines. Leafy Grass textures are exported for the ground under
them. Textures stay in the downloaded PolyHaven folders.

Outputs go to Saved/AssetImport/OpenGolfSavanna/Grass: one FBX per tile, 8-bit texture copies
(2K colour and normal maps, 1K masks) and export.json for ImportSavannaAssets.py.
"""
from pathlib import Path
import json
import math
import os
import sys

import bpy
import mathutils

HERE = Path(__file__).resolve().parent
PROJECT = HERE.parents[3]
sys.path.insert(0, str(HERE))
sys.path.insert(0, str(PROJECT / "Plugins/OpenGolfCourse/Scripts"))
import export_rules as rules
import grass_rules as grass
import png8
from art_root import art_root

ROOT = art_root()
SOURCES = {grass.MEDIUM: ROOT / f"PolyHaven/{grass.MEDIUM}/{grass.MEDIUM}_2k.blend"}
BLEND = ROOT / "Grass/Grass.blend"
OUT = PROJECT / "Saved/AssetImport/OpenGolfSavanna/Grass"
TILES = "Tiles"
PIECES = "Pieces"
CATEGORIES = {grass.MEDIUM: "Grass/Medium"}
EXPORTED = {}


def _append(source, collections=(), node_groups=()):
    with bpy.data.libraries.load(str(source), link=False) as (_available, loaded):
        loaded.collections = list(collections)
        loaded.node_groups = list(node_groups)
    return loaded


def _medium_tile(name, seed, settings, pieces, tiles):
    target = _join_pieces(name, seed, settings, pieces)
    target.name = target.data.name = name
    target["tile_m"] = list(settings["size"])
    bpy.context.scene.collection.objects.unlink(target)
    tiles.objects.link(target)


def _join_pieces(name, seed, settings, pieces):
    kinds = {piece.name: grass.medium_tuft_kind(piece.name) for piece in pieces
             if piece.name.endswith(f"_LOD{settings['lod']}")}
    parts = []
    for x, y, yaw, scale, piece in grass.tile_points(seed, kinds, settings["size"], scale=settings["scale"],
                                                    weights=settings.get("weights")):
        part = bpy.data.objects.new(f"{name}_part", bpy.data.objects[piece].data.copy())
        part.matrix_world = (mathutils.Matrix.Translation((x, y, 0)) @ mathutils.Matrix.Rotation(math.radians(yaw), 4, "Z")
                             @ mathutils.Matrix.Scale(scale, 4))
        bpy.context.scene.collection.objects.link(part)
        parts.append(part)
    target = parts[0]
    with bpy.context.temp_override(active_object=target, selected_editable_objects=parts, selected_objects=parts):
        bpy.ops.object.join()
    with bpy.context.temp_override(active_object=target, selected_editable_objects=[target], selected_objects=[target]):
        bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    return target


def build():
    for source in SOURCES.values():
        if not source.exists():
            raise RuntimeError(f"Run FetchPolyHaven.py first: {source} is missing")
    bpy.ops.wm.read_homefile(use_empty=True)
    bpy.context.preferences.filepaths.save_version = 0
    tiles = bpy.data.collections.new(TILES)
    bpy.context.scene.collection.children.link(tiles)
    # Building blocks stay in the file but out of the scene.
    pieces = bpy.data.collections.new(PIECES)
    pieces.use_fake_user = True
    lods = _append(SOURCES[grass.MEDIUM], collections=[f"grass_medium_01_LOD{lod}" for lod in range(3)]).collections
    for collection in lods:
        for obj in list(collection.objects):
            name = grass.medium_tuft(obj.name)
            if name:
                obj.name = obj.data.name = name
                obj.location = (0, 0, 0)
                pieces.objects.link(obj)
        bpy.data.collections.remove(collection)
    for name, _asset, seed, settings in grass.TILES:
        _medium_tile(name, seed, settings, list(pieces.objects), tiles)
    BLEND.parent.mkdir(parents=True, exist_ok=True)
    bpy.ops.wm.save_as_mainfile(filepath=str(BLEND), compress=True, relative_remap=True, check_existing=False)
    print("GRASS_PROJECT_BUILT", BLEND)


def export_texture(source, role):
    if source in EXPORTED:
        return EXPORTED[source]
    if not Path(source).exists():
        raise RuntimeError(f"Texture missing: {source}; run FetchPolyHaven.py")
    image = png8.load(source, role.startswith("albedo"))
    if role.startswith("albedo"):
        png8.premultiply(image)
    size = rules.export_size(*image.size, rules.EXPORT_LIMITS[role])
    if tuple(image.size) != size:
        image.scale(*size)
    target = OUT / "textures" / (Path(source).stem + ".png")
    target.parent.mkdir(parents=True, exist_ok=True)
    png8.save(image, target, role.startswith("albedo"))
    bpy.data.images.remove(image)
    EXPORTED[source] = str(target)
    return EXPORTED[source]


def stats(obj):
    """Triangles, bounds (metres around the tile centre) and materials of the evaluated tile."""
    evaluated = obj.evaluated_get(bpy.context.evaluated_depsgraph_get())
    mesh = evaluated.to_mesh()
    mesh.calc_loop_triangles()
    points = [obj.matrix_world @ v.co for v in mesh.vertices]
    materials = sorted({m.name for m in mesh.materials if m})
    result = (len(mesh.loop_triangles), [[round(min(p[i] for p in points), 3) for i in range(3)],
                                         [round(max(p[i] for p in points), 3) for i in range(3)]], materials)
    evaluated.to_mesh_clear()
    return result


def export_tile(obj):
    bpy.ops.object.select_all(action="DESELECT")
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj
    fbx = OUT / "meshes" / f"{obj.name}.fbx"
    fbx.parent.mkdir(parents=True, exist_ok=True)
    bpy.ops.export_scene.fbx(filepath=str(fbx), use_selection=True, object_types={"MESH"}, add_leaf_bones=False,
                             axis_forward="-Y", axis_up="Z", bake_anim=False, mesh_smooth_type="FACE",
                             use_mesh_modifiers=True)
    triangles, bounds, materials = stats(obj)
    if len(materials) != 1:
        raise RuntimeError(f"{obj.name}: expected one grass material, found {materials}")
    # Poly Haven sinks tuft bases a few centimetres so they meet uneven ground.
    if bounds[0][2] < -0.05:
        raise RuntimeError(f"{obj.name}: extends {bounds[0][2]} m below the tile")
    return dict(name=obj.name, kind="grass", fbx=str(fbx), triangles=triangles, tile_m=list(obj["tile_m"]),
                bounds_m=bounds, height_m=bounds[1][2], materials=materials)


def export():
    bpy.ops.wm.open_mainfile(filepath=str(BLEND))
    print("GRASS_PROJECT_OPENED", BLEND)
    OUT.mkdir(parents=True, exist_ok=True)
    meshes = [export_tile(obj) for obj in sorted(bpy.data.collections[TILES].objects, key=lambda o: o.name)]
    materials = {}
    for asset in CATEGORIES:
        sources = grass.grass_maps(asset, ROOT / f"PolyHaven/{asset}/maps")
        layer = {role: export_texture(path, role) for role, path in sources.items()}
        # The ground under the tiles is tinted to this so gaps and the cull fade don't show.
        materials[asset] = {"kind": "grass", "category": CATEGORIES[asset], "sources": sources, "layers": [layer],
                            "average_color": png8.mean_srgb(layer["albedo"])}
    # Ground under the tiles: textures only; the ground material samples them.
    sources = grass.underlay_maps(ROOT / f"PolyHaven/{grass.UNDERLAY}/maps")
    materials[grass.UNDERLAY] = {"kind": "textures", "category": "Ground/LeafyGrass", "sources": sources,
                                 "layers": [{role: export_texture(path, role) for role, path in sources.items()}]}
    manifest = {"source_project": str(BLEND), "sources": {k: str(v) for k, v in SOURCES.items()},
                "licence": "CC0 1.0 (Poly Haven)", "normal_convention": "OpenGL", "category": "Grass",
                "meshes": meshes, "materials": materials}
    (OUT / "export.json").write_text(json.dumps(manifest, indent=2))
    print("GRASS_EXPORT_COMPLETE", len(meshes), "tiles", {m["name"]: (m["triangles"], m["height_m"]) for m in meshes},
          "average", {name: entry.get("average_color") for name, entry in materials.items()})


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
