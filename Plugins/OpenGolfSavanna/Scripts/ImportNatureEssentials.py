# SPDX-License-Identifier: MIT
"""Import the Nature Essentials (CC BY 4.0) understory pieces into /OpenGolfSavanna.

    py "<project>/Plugins/OpenGolfSavanna/Scripts/ImportNatureEssentials.py" [--materials-only | --textures-only]

Export first with blender/ExportNatureEssentials.py, which writes FBX files and 2K texture copies
to Saved/AssetImport/NatureEssentials; the editable Blender project stays under the Blender art
root. Content goes to type-first NatureEssentials folders (Meshes, Materials and Textures), so
the attributed pack stays identifiable. Cards and rocks use the shared savanna parents
(M_SavannaCard, M_SavannaSolid); the grass uses M_SavannaUnderstoryGrass, built here.
--materials-only keeps the imported textures and meshes and rebuilds only the materials.
--textures-only re-imports only the textures in place, keeping tuned material instances and meshes.
Credit and licence: NOTICE.md.
"""
from pathlib import Path
import importlib
import json
import sys

import unreal as ue

PROJECT = Path(ue.Paths.project_dir()).resolve()
HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import savanna_assets as s

s = importlib.reload(s)
a = s.a
EXPORT = PROJECT / "Saved/AssetImport/NatureEssentials/export.json"
CATEGORY = "NatureEssentials"
TEXTURES, MATERIALS, MESHES = (f"{kind}/{CATEGORY}" for kind in ("Textures", "Materials", "Meshes"))
CARD = "/OpenGolfSavanna/Materials/M_SavannaCard"
SOLID = "/OpenGolfSavanna/Materials/M_SavannaSolid"
WHITE_MASK = "/OpenGolfSavanna/Materials/T_WhiteMask"
# Straw tufts against the red soil; the grass material shades by blade-texture brightness.
GRASS_COLORS = {"DryColor": "A99468", "GreenColor": "6F7A45"}
# Dry oak litter is greyer than the pack's autumn leaves.
SATURATION = {"NE_OakLeaf": 0.55, "NE_Stick": 0.7, "NE_Branch": 0.7, "NE_MossDead": 0.8, "NE_OakSeedling": 0.8}
# Starting looks for the dry savanna; tune in the MIs after review.
TINTS = {"NE_OakLeaf": "C8B49A", "NE_Stick": "D8CCBC", "NE_Branch": "D8CCBC", "NE_MossDead": "C9BFA8",
         "NE_OakSeedling": "C9C2A0", "NE_RockDark_A": "E6D5C6", "NE_RockDark_B": "E6D5C6",
         "NE_RockLight_A": "E8D6C4", "NE_RockLight_B": "E8D6C4"}


def grass_material():
    """Masked grass shaded by its blade texture's brightness, dry by default.

    A course can set CourseMask (white near play), MaskBounds (terrain-local minimum and size in
    metres), TerrainFrame (origin x, y in cm and forward x, y) and UseCourseMask = 1 to turn the
    grass green near play. Wind is the course kit's vegetation wind, off until an instance turns it on.
    """
    material = a.material("Materials", "M_SavannaUnderstoryGrass")
    for key, value in (("two_sided", True), ("tangent_space_normal", False), ("used_with_instanced_static_meshes", True),
                       ("blend_mode", ue.BlendMode.BLEND_MASKED), ("opacity_mask_clip_value", 0.4)):
        material.set_editor_property(key, value)
    world = a.node(material, ue.MaterialExpressionWorldPosition, "WorldPosition")
    uv = a.node(material, ue.MaterialExpressionTextureCoordinate, "BladeUV")
    atlas = s.sample(material, "BladeAtlas", "/Engine/EngineResources/WhiteSquareTexture", uv)
    frame = a.vector(material, "TerrainFrame", (0.0, 0.0, 1.0, 0.0), "Course")
    bounds = a.vector(material, "MaskBounds", (0.0, 0.0, 1.0, 1.0), "Course")
    mask_uv = a.custom(material, "CourseMaskUV",
                       "float2 q=(P.xy-F.xy)/100.0; float2 f=normalize(F.zw);\n"
                       "float2 m=float2(dot(q,f),dot(q,float2(-f.y,f.x)));\n"
                       "return (m-B.xy)/max(B.zw,0.001);",
                       {"P": (world, ""), "F": (frame, "RGBA"), "B": (bounds, "RGBA")},
                       ue.CustomMaterialOutputType.CMOT_FLOAT2)
    cover = s.sample(material, "CourseMask", WHITE_MASK, mask_uv, mask=True)
    green = a.vector(material, "GreenColor", a.linear_color("5E6B3B"), "Grass")
    dry = a.vector(material, "DryColor", a.linear_color("7E6B4F"), "Grass")
    color = a.custom(material, "BladeColor",
                     "float shade=dot(Atlas,float3(0.2126,0.7152,0.0722));"
                     "return lerp(Dry,Green,Mask*Use)*(0.86+shade*0.28);",
                     {"Atlas": (atlas, "RGB"), "Mask": (cover, "R"), "Green": (green, "RGB"), "Dry": (dry, "RGB"),
                      "Use": (a.scalar(material, "UseCourseMask", 0, "Course"), "")},
                     ue.CustomMaterialOutputType.CMOT_FLOAT3)
    a.output(material, color, ue.MaterialProperty.MP_BASE_COLOR)
    a.output(material, atlas, ue.MaterialProperty.MP_OPACITY_MASK, "A")
    a.output(material, a.node(material, ue.MaterialExpressionConstant3Vector, "UpNormal",
                             constant=ue.LinearColor(0, 0, 1, 1)), ue.MaterialProperty.MP_NORMAL)
    a.output(material, a.scalar(material, "Roughness", 0.95, "Grass"), ue.MaterialProperty.MP_ROUGHNESS)
    a.output(material, a.scalar(material, "Specular", 0.06, "Grass"), ue.MaterialProperty.MP_SPECULAR)
    s.wind.add_vegetation_wind(material)
    return a.finish_material(material)


def import_texture(path, role, normal=None, reuse=False):
    # The pack's normal maps are OpenGL.
    return s.import_texture(path, TEXTURES, role, normal, opengl_normals=True, reuse=reuse, prefix="T_NE_")


def material_textures(roles, reuse=False):
    """One material's textures by parameter name."""
    normal = import_texture(roles["normal"], "normal", reuse=reuse) if "normal" in roles else None
    textures = {"Albedo": import_texture(roles["albedo"], "albedo", reuse=reuse)}
    if normal:
        textures["Normal"] = normal
    for role, parameter in (("roughness", "Roughness"), ("ao", "AmbientOcclusion"), ("opacity", "Opacity")):
        if role in roles:
            textures[parameter] = import_texture(roles[role], role, normal, reuse=reuse)
    return textures


def build_materials(manifest, kinds, reuse=False):
    card, solid, grass = s.required(CARD), s.required(SOLID), grass_material()
    result = {}
    for name, roles in manifest["materials"].items():
        textures = material_textures(roles, reuse)
        if name == "NE_Grass":
            colors = {key: a.linear_color(value) for key, value in GRASS_COLORS.items()}
            result[name] = s.instance(MATERIALS, "MI_" + name, grass, {"BladeAtlas": textures["Albedo"]}, colors)
            continue
        settings = {"Tint": a.linear_color(TINTS.get(name, "FFFFFF"))}
        if kinds[name] == "rock":
            low, high = roles.get("roughness_ramp", [[0, 0], [1, 1]])
            settings.update(RampLow=low[0], RampHigh=high[0])
            result[name] = s.instance(MATERIALS, "MI_" + name, solid, textures, settings)
        else:
            settings["OpacityFromAlbedoAlpha"] = 1.0 if "opacity_alpha" in roles else 0.0
            settings["Saturation"] = SATURATION.get(name, 1.0)
            result[name] = s.instance(MATERIALS, "MI_" + name, card, textures, settings)
    return result


def import_mesh(item, materials):
    mesh = a.import_asset(item["fbx"], MESHES, item["name"], mesh=True)
    slots = mesh.get_editor_property("static_materials")
    for index, slot in enumerate(slots):
        slot_name = str(slot.get_editor_property("material_slot_name"))
        key = next((name for name in materials if slot_name == name or slot_name.startswith(name + "_")), None)
        if not key:
            raise RuntimeError(f"{item['name']}: no material for slot {slot_name}")
        mesh.set_material(index, materials[key])
    nanite = mesh.get_editor_property("nanite_settings")
    if not nanite.get_editor_property("enabled"):
        nanite.set_editor_property("enabled", True)
        mesh.set_editor_property("nanite_settings", nanite)
    body = mesh.get_editor_property("body_setup")
    if body:
        body.set_editor_property("collision_trace_flag", ue.CollisionTraceFlag.CTF_USE_DEFAULT)
    ue.log(f"NE_MESH {item['name']} slots={[str(x.get_editor_property('material_slot_name')) for x in slots]}")
    return a.save(mesh)


def load_manifest():
    if not EXPORT.exists():
        raise RuntimeError(f"Run blender/ExportNatureEssentials.py first: {EXPORT} is missing")
    manifest = json.loads(EXPORT.read_text())
    kinds = {name: item["kind"] for item in manifest["meshes"] for name in item["materials"]}
    return manifest, kinds


def main(argv):
    materials_only = "--materials-only" in argv
    manifest, kinds = load_manifest()
    if "--textures-only" in argv:
        with s.savanna_helpers():
            textures = {texture.get_path_name() for roles in manifest["materials"].values()
                        for texture in material_textures(roles).values()}
        ue.log(f"NE_IMPORT_COMPLETE textures={len(textures)} textures_only=True")
        return
    with s.savanna_helpers():
        materials = build_materials(manifest, kinds, reuse=materials_only)
        meshes = [] if materials_only else [import_mesh(item, materials) for item in manifest["meshes"]]
        # Rebuilt instances get their wind back (vegetation_wind_assignments.py).
        import ApplyVegetationWind
        importlib.reload(ApplyVegetationWind).apply_materials([mi.get_path_name() for mi in materials.values()])
    ue.log(f"NE_IMPORT_COMPLETE meshes={len(meshes)} materials={len(materials)} materials_only={materials_only}")


if __name__ == "__main__":
    main(sys.argv[1:])
