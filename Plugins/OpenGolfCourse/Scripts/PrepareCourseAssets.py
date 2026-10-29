#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Download pinned CC0 inputs; convert with Blender --background --python this_file.

With ordinary Python this downloads/validates only. With Blender it also exports
the course-kit meshes. Outputs are scratch files under the project's
Saved/AssetImport/Prepared; BuildCourseAssets.py imports them into the plugin.
The editable .blend projects live outside the repository in <art root>/OpenGolfCourse
(see art_root.py; pass `-- --art-root PATH` to choose another folder).
"""
from pathlib import Path
import hashlib
import os
import sys
import urllib.request
import zipfile

sys.path.insert(0, str(Path(__file__).resolve().parent))
from art_root import art_root

# Plugins/OpenGolfCourse/Scripts/this_file -> project root.
ROOT = Path(__file__).resolve().parents[3]
WORK = ROOT / "Saved/AssetImport"
OUT = WORK / "Prepared"
# Editable Blender sources, kept outside the repository.
ART = art_root() / "OpenGolfCourse"
USER_AGENT = "UOpenGolf-PrepareCourseAssets/1 (+https://github.com/digitalhand/uopengolf)"
SOURCES = [
    ("Grass004_2K-JPG.zip", "https://ambientcg.com/get?file=Grass004_2K-JPG.zip",
     "d7d3f08e8bb1e59aed0c3dbb85ea0b46d5308481f89b6a3fd62c0ec9e862245b"),
    ("golf_0.zip", "https://opengameart.org/sites/default/files/golf_0.zip",
     "33358f1d9549e3b56f8826b65982dc1b5bafa701321425e7970971b7e4388d2e"),
]


def prepare():
    OUT.mkdir(parents=True, exist_ok=True)
    for name, url, digest in SOURCES:
        path = WORK / name
        if not path.exists():
            # ambientCG rejects Python's default User-Agent (HTTP 403); checksums still apply.
            request = urllib.request.Request(url, headers={"User-Agent": USER_AGENT})
            with urllib.request.urlopen(request) as response:
                path.write_bytes(response.read())
        if hashlib.sha256(path.read_bytes()).hexdigest() != digest:
            raise RuntimeError(f"Source checksum mismatch: {path}")
    with zipfile.ZipFile(WORK / SOURCES[0][0]) as archive:
        for suffix in ("Color", "NormalDX", "Roughness"):
            name = f"Grass004_2K-JPG_{suffix}.jpg"
            (OUT / name).write_bytes(archive.read(name))
    with zipfile.ZipFile(WORK / "golf_0.zip") as archive:
        for name in ("Golf_ball.obj", "Golf_ball.mtl"):
            (OUT / name).write_bytes(archive.read(name))


def build_ball(bpy):
    from mathutils import Vector
    bpy.ops.wm.obj_import(filepath=str(OUT / "Golf_ball.obj"), forward_axis="NEGATIVE_Z", up_axis="Y")
    bpy.ops.object.join()
    obj = bpy.context.object
    obj.name = "GolfBall"
    points = [obj.matrix_world @ v.co for v in obj.data.vertices]
    lower = Vector(tuple(min(v[i] for v in points) for i in range(3)))
    upper = Vector(tuple(max(v[i] for v in points) for i in range(3)))
    center = (lower + upper) * 0.5
    diameter = max(upper - lower)
    # Blender meters -> FBX centimeters: 1 m source diameter gives UE's 100 cm.
    for v, p in zip(obj.data.vertices, points):
        v.co = (p - center) / diameter
    obj.matrix_world.identity()
    obj.data.materials.clear()
    for poly in obj.data.polygons:
        poly.use_smooth = True
    print("Ball triangles:", sum(len(p.vertices) - 2 for p in obj.data.polygons))


def build_flagstick(bpy):
    # The pole alone, with explicit dimensions in meters. Ground is Z=0; the 10 cm buried section is
    # excluded from the seven-foot visible height. The flag is cloth simulated by
    # UOpenGolfFlagstickComponent, not part of this mesh.
    bpy.ops.mesh.primitive_cylinder_add(vertices=32, radius=0.00635, depth=2.2336,
                                      location=(0, 0, 1.0168))
    obj = bpy.context.object
    obj.name = "Flagstick"
    obj.data.materials.append(bpy.data.materials.new("Pole"))
    for poly in obj.data.polygons:
        poly.use_smooth = len(poly.vertices) == 4
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)


MESHES = (("SM_GolfBall", build_ball), ("SM_Flagstick", build_flagstick))


def convert(regenerate=False):
    """Export each mesh from its editable .blend in ART, creating the .blend when missing.

    Edit a .blend in Blender and rerun this to export your change; --regenerate rebuilds the
    .blend files from the pinned sources and the generators above, discarding edits.
    """
    import bpy
    ART.mkdir(parents=True, exist_ok=True)
    for name, build in MESHES:
        blend = ART / (name + ".blend")
        if blend.exists() and not regenerate:
            bpy.ops.wm.open_mainfile(filepath=str(blend))
        else:
            bpy.ops.wm.read_homefile(use_empty=True)
            build(bpy)
            bpy.ops.wm.save_as_mainfile(filepath=str(blend), compress=True, check_existing=False)
            print("Saved", blend)
        bpy.ops.object.select_all(action="DESELECT")
        meshes = [obj for obj in bpy.context.scene.objects if obj.type == "MESH"]
        if not meshes:
            raise RuntimeError(f"No mesh objects in {blend}")
        for obj in meshes:
            obj.select_set(True)
        bpy.context.view_layer.objects.active = meshes[0]
        # Bake object rotation and scale into the export; the .blend on disk is not resaved.
        bpy.ops.object.transform_apply(location=False, rotation=True, scale=True)
        bpy.ops.export_scene.fbx(filepath=str(OUT / (name + ".fbx")), use_selection=True,
                                 object_types={"MESH"}, add_leaf_bones=False,
                                 axis_forward="-Y", axis_up="Z", bake_anim=False, mesh_smooth_type="FACE")
        print("Exported", OUT / (name + ".fbx"))


if __name__ == "__main__":
    prepare()
    try:
        import bpy
    except ImportError:
        print("Inputs verified. Run this script with Blender to export meshes.")
    else:
        # Blender passes script arguments after "--".
        convert(regenerate="--regenerate" in sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else False)
        # Blender 5.0.1 on Linux can hang on exit in background mode, even through
        # wm.quit_blender. Every file is written and closed by now, so leave immediately.
        sys.stdout.flush()
        os._exit(0)
