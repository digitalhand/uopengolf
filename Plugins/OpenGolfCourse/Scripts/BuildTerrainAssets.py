# SPDX-License-Identifier: MIT
"""Build only new coastal materials, vegetation, and presets; preserve existing course assets."""
from pathlib import Path
import sys

import unreal as ue

sys.path.insert(0, str(Path(__file__).resolve().parent))
import course_assets as assets

OWNER = "OpenGolfCourse.Terrain.v1"
FOLDER = "Terrain/Materials"


def create_data(name: str, asset_class, folder: str = "Terrain/Presets"):
    factory = ue.DataAssetFactory()
    factory.set_editor_property("data_asset_class", asset_class)
    return assets.load_or_create(folder, name, asset_class, factory)


def value_noise(cell_cm, seed=0):
    """HLSL for smooth value noise in [0, 1] over P.xy, with cells cell_cm wide.

    Corners are hashed from integer cell coordinates, so neighbouring cells agree exactly on
    their shared corners. A frac(sin(x) * 43758) hash amplifies rounding differences between
    cells into straight seams along the cell grid. The seed offsets the cells, so noises of
    the same size can differ.
    """
    return (f"float2 q=P.xy/{float(cell_cm)}; float2 i=floor(q); float2 f=frac(q); f=f*f*(3-2*f);\n"
            f"int2 c=int2(i)+{32768 + 7919 * seed};\n"
            "uint4 h=uint4(c.x,c.x+1,c.x,c.x+1)*73856093u^uint4(c.y,c.y,c.y+1,c.y+1)*19349663u;\n"
            "h=(h^61u)^(h>>16); h*=9u; h^=h>>4; h*=0x27d4eb2du; h^=h>>15;\n"
            "float4 v=float4(h>>8)/16777215.0;\n"
            "return lerp(lerp(v.x,v.y,f.x),lerp(v.z,v.w,f.x),f.y);")


# Smooth 19 m variation shared by the course surfaces.
MACRO_VARIATION = value_noise(1900)


def build_material(name="M_CoastalSurface"):
    material = assets.material(FOLDER, name)
    position = assets.node(material, ue.MaterialExpressionWorldPosition, "Position")
    size = assets.scalar(material, "TextureSizeCm", 120, "Surface")
    uv = assets.custom(material, "UV", "return P.xy / max(S, 1.0);",
                       {"P": (position, ""), "S": (size, "")}, ue.CustomMaterialOutputType.CMOT_FLOAT2)
    color = assets.node(material, ue.MaterialExpressionTextureSampleParameter2D, "TurfTexture",
                        parameter_name="TurfTexture", texture=ue.load_asset("/OpenGolfCourse/Surfaces/T_Grass_Color"),
                        sampler_type=ue.MaterialSamplerType.SAMPLERTYPE_COLOR)
    normal = assets.node(material, ue.MaterialExpressionTextureSampleParameter2D, "TurfNormal",
                         parameter_name="TurfNormal", texture=ue.load_asset("/OpenGolfCourse/Surfaces/T_Grass_NormalDX"),
                         sampler_type=ue.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    assets.connect(uv, color, "UVs")
    assets.connect(uv, normal, "UVs")
    low = assets.vector(material, "LowColor", assets.linear_color("586447"), "Surface")
    high = assets.vector(material, "HighColor", assets.linear_color("7C855B"), "Surface")
    macro = assets.custom(material, "MacroVariation", MACRO_VARIATION,
                          {"P": (position, "")}, ue.CustomMaterialOutputType.CMOT_FLOAT1)
    palette = assets.custom(material, "SurfaceColor",
                            "float fine=dot(C,float3(0.2126,0.7152,0.0722));\n"
                            "float stripe=0.5+0.5*sin(P.y/400.0*3.14159265);\n"
                            "float band=Contrast*(stripe-0.5)*(1-smoothstep(40.0,200.0,fwidth(P.y)));\n"
                            "return lerp(L,H,saturate(0.25+Macro*0.35+(fine-0.3)*Detail+band));",
                            {"C": (color, "RGB"), "P": (position, ""), "L": (low, "RGB"), "H": (high, "RGB"),
                             "Macro": (macro, ""), "Detail": (assets.scalar(material, "DetailStrength", 0.5, "Surface"), ""),
                             "Contrast": (assets.scalar(material, "StripeContrast", 0, "Surface"), "")},
                            ue.CustomMaterialOutputType.CMOT_FLOAT3)
    assets.output(material, palette, ue.MaterialProperty.MP_BASE_COLOR)
    blended_normal = assets.custom(material, "NormalStrength", "return normalize(lerp(float3(0,0,1),N,Strength));",
                                   {"N": (normal, "RGB"), "Strength": (assets.scalar(material, "NormalStrength", 0.3, "Surface"), "")},
                                   ue.CustomMaterialOutputType.CMOT_FLOAT3)
    assets.output(material, blended_normal, ue.MaterialProperty.MP_NORMAL)
    assets.output(material, assets.scalar(material, "Roughness", 0.88, "Surface"), ue.MaterialProperty.MP_ROUGHNESS)
    assets.output(material, assets.scalar(material, "Specular", 0.18, "Surface"), ue.MaterialProperty.MP_SPECULAR)
    return assets.finish_material(material)


def build_grass_material():
    material = assets.material(FOLDER, "M_CoastalGrass")
    material.set_editor_property("two_sided", True)
    material.set_editor_property("used_with_instanced_static_meshes", True)
    material.set_editor_property("tangent_space_normal", False)
    assets.output(material, assets.node(material, ue.MaterialExpressionConstant3Vector, "GrassUp",
                                       constant=ue.LinearColor(0, 0, 1, 1)), ue.MaterialProperty.MP_NORMAL)
    uv = assets.node(material, ue.MaterialExpressionTextureCoordinate, "BladeUV")
    low = assets.vector(material, "RootColor", assets.linear_color("747653"), "Grass")
    high = assets.vector(material, "TipColor", assets.linear_color("8D8B61"), "Grass")
    color = assets.custom(material, "BladeColor", "return lerp(L,H,smoothstep(0.05,1.0,UV.y));",
                          {"UV": (uv, ""), "L": (low, "RGB"), "H": (high, "RGB")}, ue.CustomMaterialOutputType.CMOT_FLOAT3)
    assets.output(material, color, ue.MaterialProperty.MP_BASE_COLOR)
    assets.output(material, assets.scalar(material, "Roughness", 0.9, "Grass"), ue.MaterialProperty.MP_ROUGHNESS)
    assets.output(material, assets.scalar(material, "Specular", 0.1, "Grass"), ue.MaterialProperty.MP_SPECULAR)
    return assets.finish_material(material)


def build_tee_material(parent):
    tee = assets.material_instance(FOLDER, "MI_CoastalTee", parent)
    for name, value in {"StripeContrast": 0, "NormalStrength": 0.08, "Roughness": 0.95, "Specular": 0.1}.items():
        assets.MAT.set_material_instance_scalar_parameter_value(tee, name, value)
    for name, color in (("LowColor", "486136"), ("HighColor", "617A43")):
        assets.MAT.set_material_instance_vector_parameter_value(tee, name, ue.LinearColor(*assets.linear_color(color)))
    return assets.save(tee)


def main():
    expected = [f"/OpenGolfCourse/Terrain/Presets/DA_Coastal{name}" for name in ("Fairway", "Green", "Fringe", "Rough", "Sand", "GrassVegetation", "RoughVegetation")]
    if all(assets.LIB.does_asset_exist(path) for path in expected):
        ue.log("Coastal presets already exist; preserving their authored settings.")
        return
    previous_owner = assets.OWNER
    assets.OWNER = OWNER
    try:
        parent = build_material()
        build_tee_material(parent)
        settings = {
            "Fairway": ("526D39", "78904F", 110, 0.2, 0.09),
            "Green": ("57733E", "7C9556", 45, 0.08, 0),
            "Fringe": ("52663B", "788650", 80, 0.16, 0),
            "Rough": ("646847", "969366", 160, 0.4, 0),
            "Sand": ("B4A68C", "D1C2A5", 70, 0.12, 0),
        }
        for surface, (low, high, size, strength, stripes) in settings.items():
            instance = assets.material_instance(FOLDER, f"MI_Coastal{surface}", parent)
            for key, value in {"TextureSizeCm": size, "NormalStrength": strength, "StripeContrast": stripes,
                               "DetailStrength": 0.18 if surface == "Sand" else 0.5}.items():
                assets.MAT.set_material_instance_scalar_parameter_value(instance, key, value)
            for key, value in (("LowColor", low), ("HighColor", high)):
                assets.MAT.set_material_instance_vector_parameter_value(instance, key, ue.LinearColor(*assets.linear_color(value)))
            instance.set_editor_property("override_phys_material", True)
            instance.set_editor_property("phys_material", ue.load_asset(f"/OpenGolfCourse/Surfaces/PM_{surface}"))
            assets.MAT.update_material_instance(instance)
            assets.save(instance)
            preset = create_data(f"DA_Coastal{surface}", ue.OpenGolfSurfacePreset)
            preset.set_editor_property("surface", getattr(ue.OpenFairwaySurface, surface.upper()))
            preset.set_editor_property("material", instance)
            assets.save(preset)
        grass_material = build_grass_material()
        for name, spacing, scale, cull in (("CoastalGrass", 2.6, (0.7, 1.25), 130),
                                           ("CoastalRough", 2.0, (0.8, 1.2), 65)):
            mesh = assets.import_asset(f"SM_{name}.fbx", "Terrain/Vegetation", f"SM_{name}", mesh=True)
            mesh.set_material(0, grass_material)
            assets.save(mesh)
            preset = create_data(f"DA_{name}Vegetation", ue.OpenGolfVegetationPreset)
            preset.set_editor_property("meshes", [mesh])
            preset.set_editor_property("surface", ue.OpenFairwaySurface.ROUGH)
            preset.set_editor_property("spacing_m", spacing)
            preset.set_editor_property("scale_range", ue.Vector2D(*scale))
            preset.set_editor_property("cull_distance_m", cull)
            preset.set_editor_property("boundary_clearance_m", 1.2)
            assets.save(preset)
        ue.log("OPEN_GOLF_TERRAIN_ASSETS_COMPLETE")
    finally:
        assets.OWNER = previous_owner


if __name__ == "__main__":
    main()
