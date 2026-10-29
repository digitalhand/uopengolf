# SPDX-License-Identifier: MIT
"""Unreal editor helpers for the open-licence savanna content in /OpenGolfSavanna.

Builders are idempotent like the course kit's (see course_assets.py): graph nodes are found by
their description and updated in place. Shared parents:
  M_SavannaCard   masked two-sided cards (leaves, litter, moss): mostly upward shading with
                  normal detail fading over distance, the grass stability treatment for cover.
  M_SavannaSolid  opaque surfaces (bark, rocks), optionally blending a second texture layer by
                  the mesh's vertex colour red channel (Poly Haven's trunk does this).
Both carry the course kit's vegetation wind, off until an instance turns it on (ApplyVegetationWind.py).
"""
from contextlib import contextmanager
from pathlib import Path
import importlib
import re
import struct
import sys

import unreal as ue

PROJECT = Path(ue.Paths.project_dir()).resolve()
PLUGIN = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(PROJECT / "Plugins/OpenGolfCourse/Scripts"))
import course_assets as a

a = importlib.reload(a)
import vegetation_wind_assets as wind  # After course_assets, which it shares.

wind = importlib.reload(wind)
MOUNT = "/OpenGolfSavanna"
OWNER = "OpenGolfSavanna.v1"
WHITE = "/Engine/EngineResources/WhiteSquareTexture"
FLAT_NORMAL = "/Engine/EngineMaterials/DefaultNormal"
WHITE_MASK = f"{MOUNT}/Materials/T_WhiteMask"
MAX_SIZE = {"albedo": 2048, "albedo_dry": 2048, "normal": 2048, "roughness": 1024, "ao": 1024, "opacity": 1024}


@contextmanager
def course_helpers(mount, content_dir, owner):
    """Point the course-kit helpers at another content root (a plugin mount or /Game) and generator tag."""
    previous = a.MOUNT, a.CONTENT_DIR, a.OWNER
    a.MOUNT, a.CONTENT_DIR, a.OWNER = mount, Path(content_dir), owner
    try:
        yield a
    finally:
        a.MOUNT, a.CONTENT_DIR, a.OWNER = previous


def savanna_helpers():
    """Point the course-kit helpers at this plugin while building its assets."""
    return course_helpers(MOUNT, PLUGIN / "Content", OWNER)


def required(path):
    asset = ue.EditorAssetLibrary.load_asset(path)
    if not asset:
        raise RuntimeError(f"Missing required asset: {path}")
    return asset


def sample(material, name, path, uv, normal=False, mask=False):
    kind = (ue.MaterialSamplerType.SAMPLERTYPE_NORMAL if normal else
            ue.MaterialSamplerType.SAMPLERTYPE_MASKS if mask else ue.MaterialSamplerType.SAMPLERTYPE_COLOR)
    expression = a.node(material, ue.MaterialExpressionTextureSampleParameter2D, name,
                        parameter_name=name, texture=required(path), sampler_type=kind)
    a.connect(uv, expression, "UVs")
    return expression


def texture_name(path, prefix="T_"):
    return prefix + re.sub(r"[^A-Za-z0-9]+", "_", Path(path).stem).strip("_")


def import_texture(path, folder, role, normal=None, opengl_normals=True, reuse=False, prefix="T_"):
    """Import a texture with settings for its role; reuse an existing import when asked."""
    existing = a.asset_path(folder, texture_name(path, prefix))
    if reuse and ue.EditorAssetLibrary.does_asset_exist(existing):
        return required(existing)
    texture = a.import_asset(path, folder, texture_name(path, prefix))
    texture.set_editor_property("srgb", role.startswith("albedo"))
    texture.set_editor_property("max_texture_size", MAX_SIZE[role])
    if role == "normal":
        texture.set_editor_property("compression_settings", ue.TextureCompressionSettings.TC_NORMALMAP)
        texture.set_editor_property("flip_green_channel", opengl_normals)
    elif role != "albedo":
        texture.set_editor_property("compression_settings", ue.TextureCompressionSettings.TC_MASKS)
    if role == "roughness" and normal:
        # Raise roughness where the normal map's detail averages out, so distant surfaces don't sparkle.
        texture.set_editor_property("composite_texture", normal)
        texture.set_editor_property("composite_texture_mode", ue.CompositeTextureMode.CTM_NORMAL_ROUGHNESS_TO_GREEN)
    return a.save(texture)


def white_mask(scratch):
    """A linear white default for Masks samplers (the engine's white square is sRGB)."""
    source = Path(scratch) / "T_WhiteMask.tga"
    source.parent.mkdir(parents=True, exist_ok=True)
    source.write_bytes(struct.pack("<BBBHHBHHHHBB", 0, 0, 2, 0, 0, 0, 0, 0, 4, 4, 24, 0x20) + b"\xff" * 48)
    texture = a.import_asset(str(source), "Materials", "T_WhiteMask")
    texture.set_editor_property("srgb", False)
    texture.set_editor_property("compression_settings", ue.TextureCompressionSettings.TC_MASKS)
    return a.save(texture)


def card_material():
    material = a.material("Materials", "M_SavannaCard")
    for key, value in (("two_sided", True), ("tangent_space_normal", False), ("used_with_instanced_static_meshes", True),
                       ("used_with_nanite", True), ("blend_mode", ue.BlendMode.BLEND_MASKED), ("opacity_mask_clip_value", 0.45)):
        material.set_editor_property(key, value)
    uv = a.node(material, ue.MaterialExpressionTextureCoordinate, "CardUV")
    albedo = sample(material, "Albedo", WHITE, uv)
    opacity = sample(material, "Opacity", WHITE_MASK, uv, mask=True)
    normal = sample(material, "Normal", FLAT_NORMAL, uv, normal=True)
    rough = sample(material, "Roughness", WHITE_MASK, uv, mask=True)
    ao = sample(material, "AmbientOcclusion", WHITE_MASK, uv, mask=True)
    world_normal = a.node(material, ue.MaterialExpressionTransform, "NormalToWorld",
                          transform_source_type=ue.MaterialVectorCoordTransformSource.TRANSFORMSOURCE_TANGENT,
                          transform_type=ue.MaterialVectorCoordTransform.TRANSFORM_WORLD)
    a.connect(normal, world_normal, "", "RGB")
    depth = a.node(material, ue.MaterialExpressionPixelDepth, "PixelDepth")
    tint = a.vector(material, "Tint", (1, 1, 1, 1), "Card")
    color = a.custom(material, "CardColor",
                     "float3 c=A*T*lerp(1,O,S)*B;"
                     "return lerp(dot(c,float3(0.2126,0.7152,0.0722)).xxx,c,Sat);",
                     {"A": (albedo, "RGB"), "T": (tint, "RGB"), "O": (ao, "R"),
                      "S": (a.scalar(material, "AOStrength", 0.6, "Card"), ""), "B": (a.scalar(material, "Brightness", 1.0, "Card"), ""),
                      "Sat": (a.scalar(material, "Saturation", 1.0, "Card"), "")},
                     ue.CustomMaterialOutputType.CMOT_FLOAT3)
    # Mostly upward shading, with normal detail only up close (the grass stability rule for cover).
    shading = a.custom(material, "CardNormal",
                       "float fade=saturate(1-(D-F0)/max(F1-F0,1));"
                       "return normalize(lerp(float3(0,0,1),N,S*fade));",
                       {"N": (world_normal, ""), "D": (depth, ""), "S": (a.scalar(material, "NormalStrength", 0.35, "Card"), ""),
                        "F0": (a.scalar(material, "NormalFadeStartCm", 800, "Card"), ""),
                        "F1": (a.scalar(material, "NormalFadeEndCm", 2500, "Card"), "")},
                       ue.CustomMaterialOutputType.CMOT_FLOAT3)
    mask = a.custom(material, "CardOpacity", "return lerp(O,A,F);",
                    {"O": (opacity, "R"), "A": (albedo, "A"), "F": (a.scalar(material, "OpacityFromAlbedoAlpha", 0, "Card"), "")},
                    ue.CustomMaterialOutputType.CMOT_FLOAT1)
    roughness = a.custom(material, "CardRoughness", "return lerp(L,H,R);",
                         {"R": (rough, "G"), "L": (a.scalar(material, "RoughnessMin", 0.7, "Card"), ""),
                          "H": (a.scalar(material, "RoughnessMax", 1.0, "Card"), "")},
                         ue.CustomMaterialOutputType.CMOT_FLOAT1)
    a.output(material, color, ue.MaterialProperty.MP_BASE_COLOR)
    a.output(material, shading, ue.MaterialProperty.MP_NORMAL)
    a.output(material, mask, ue.MaterialProperty.MP_OPACITY_MASK)
    a.output(material, roughness, ue.MaterialProperty.MP_ROUGHNESS)
    a.output(material, a.scalar(material, "Specular", 0.2, "Card"), ue.MaterialProperty.MP_SPECULAR)
    wind.add_vegetation_wind(material)
    return a.finish_material(material)


def solid_material():
    material = a.material("Materials", "M_SavannaSolid")
    material.set_editor_property("used_with_instanced_static_meshes", True)
    material.set_editor_property("used_with_nanite", True)
    uv = a.node(material, ue.MaterialExpressionTextureCoordinate, "SolidUV")
    albedo = sample(material, "Albedo", WHITE, uv)
    normal = sample(material, "Normal", FLAT_NORMAL, uv, normal=True)
    rough = sample(material, "Roughness", WHITE_MASK, uv, mask=True)
    ao = sample(material, "AmbientOcclusion", WHITE_MASK, uv, mask=True)
    albedo_b = sample(material, "AlbedoB", WHITE, uv)
    normal_b = sample(material, "NormalB", FLAT_NORMAL, uv, normal=True)
    rough_b = sample(material, "RoughnessB", WHITE_MASK, uv, mask=True)
    vertex = a.node(material, ue.MaterialExpressionVertexColor, "VertexColor")
    # Second layer weight: the vertex colour's red channel, when VertexBlend is 1.
    weight = a.custom(material, "LayerWeight", "return saturate(V*E);",
                      {"V": (vertex, "R"), "E": (a.scalar(material, "VertexBlend", 0, "Solid"), "")},
                      ue.CustomMaterialOutputType.CMOT_FLOAT1)
    tint = a.vector(material, "Tint", (1, 1, 1, 1), "Solid")
    color = a.custom(material, "SolidColor", "return lerp(A,B,W)*T*lerp(1,O,S);",
                     {"A": (albedo, "RGB"), "B": (albedo_b, "RGB"), "W": (weight, ""), "T": (tint, "RGB"), "O": (ao, "R"),
                      "S": (a.scalar(material, "AOStrength", 0.8, "Solid"), "")},
                     ue.CustomMaterialOutputType.CMOT_FLOAT3)
    shading = a.custom(material, "SolidNormal", "return normalize(lerp(float3(0,0,1),normalize(lerp(N,M,W)),S));",
                       {"N": (normal, "RGB"), "M": (normal_b, "RGB"), "W": (weight, ""),
                        "S": (a.scalar(material, "NormalStrength", 1.0, "Solid"), "")},
                       ue.CustomMaterialOutputType.CMOT_FLOAT3)
    # RampLow/High remap the roughness texture (Nature Essentials rocks use a colour ramp); defaults pass it through.
    roughness = a.custom(material, "SolidRoughness", "float r=lerp(R,Q,W);return max(M,saturate((r-L)/max(H-L,0.01)));",
                         {"R": (rough, "G"), "Q": (rough_b, "G"), "W": (weight, ""),
                          "L": (a.scalar(material, "RampLow", 0, "Solid"), ""), "H": (a.scalar(material, "RampHigh", 1, "Solid"), ""),
                          "M": (a.scalar(material, "RoughnessMin", 0.55, "Solid"), "")},
                         ue.CustomMaterialOutputType.CMOT_FLOAT1)
    a.output(material, color, ue.MaterialProperty.MP_BASE_COLOR)
    a.output(material, shading, ue.MaterialProperty.MP_NORMAL)
    a.output(material, roughness, ue.MaterialProperty.MP_ROUGHNESS)
    wind.add_vegetation_wind(material)
    return a.finish_material(material)


def instance(folder, name, parent, textures, settings):
    mi = a.material_instance(folder, name, parent)
    for parameter, texture in textures.items():
        ue.MaterialEditingLibrary.set_material_instance_texture_parameter_value(mi, parameter, texture)
    for parameter, value in settings.items():
        if isinstance(value, tuple):
            ue.MaterialEditingLibrary.set_material_instance_vector_parameter_value(mi, parameter, ue.LinearColor(*value))
        else:
            ue.MaterialEditingLibrary.set_material_instance_scalar_parameter_value(mi, parameter, value)
    ue.MaterialEditingLibrary.update_material_instance(mi)
    return a.save(mi)
