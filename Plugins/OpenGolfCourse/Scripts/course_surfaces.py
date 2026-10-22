# SPDX-License-Identifier: MIT
"""Surface assets: physical materials, mowing stripes, turf and grass-blade materials."""
import unreal as ue

from course_assets import (connect, custom, finish_function, finish_material, function_call, function_input,
                           function_output, import_asset, linear_color, load_or_create, material, material_function,
                           material_instance, node, output, save, scalar, vector, MAT)

SURFACES = ("Fairway", "FairwaySoft", "Rough", "Firm", "Green", "Sand", "Fringe")
FOLDER = "Surfaces"
GRASS_FOLDER = "Grass"

# Turf palette (sRGB). Fairway: subtle bands between two close greens.
FAIRWAY_DARK_HEX, FAIRWAY_LIGHT_HEX = "406B1F", "4D7E20"
# Rough: deeper and coarser, no mowing bands.
ROUGH_DARK_HEX, ROUGH_LIGHT_HEX = "2C4F16", "46711F"
# Green: lighter, finer bentgrass with no mowing stripes. The finer texture tile is mipmapped, and
# with no procedural bands nothing on it can alias.
GREEN_DARK_HEX, GREEN_LIGHT_HEX = "4C8228", "5A9332"
GREEN_TEXTURE_SIZE_CM, GREEN_DETAIL = 70.0, 0.08
# Nine bands across the range's 34 m fairway (AUOpenGolfRange::FairwayHalfWidthM = 17): an odd
# count centers a band on the target line and puts the fairway edges on band boundaries.
# UOpenGolf.Range.Presentation checks that the two stay in step.
STRIPE_WIDTH_CM = 3400.0 / 9.0
STRIPE_CONTRAST = 0.5
TEXTURE_SIZE_CM = 140.0

# Mowing bands run along the mow direction (StripeAngleDeg from world +X, through StripeOriginCm).
# Procedural bands get no mip filtering: toward the horizon a band narrows below a pixel and
# aliases into moire that changes with every temporal-AA jitter. The edge widens by the pixel
# footprint (fwidth, in radians of phase), and contrast fades to the mean between ~40 and ~4
# pixels per band pair.
STRIPE_CODE = """float angle = radians(AngleDeg);
float across = dot(P.xy - Origin, float2(-sin(angle), cos(angle)));
float phase = across * 3.14159265 / max(WidthCm, 1.0);
float footprint = fwidth(phase);
float edge = 0.12 + footprint;
float stripe = lerp(smoothstep(-edge, edge, cos(phase)), 0.5, saturate((footprint - 0.15) * 0.8));
return lerp(0.5, stripe, saturate(Contrast));"""


def build_physical_materials():
    """One OpenFairway physical material per surface. Needs the compiled OpenFairwayPhysics module."""
    if not hasattr(ue, "OpenFairwayPhysicalMaterial"):
        ue.log_warning("OpenFairwayPhysicalMaterial is not loaded yet: skipping PM_* until the editor is rebuilt and restarted.")
        return {}
    materials = {}
    for name in SURFACES:
        factory = ue.PhysicalMaterialFactoryNew()
        factory.set_editor_property("physical_material_class", ue.OpenFairwayPhysicalMaterial)
        physical = load_or_create(FOLDER, f"PM_{name}", ue.OpenFairwayPhysicalMaterial, factory)
        enum_name = "".join("_" + c if c.isupper() and i else c for i, c in enumerate(name)).upper()
        physical.set_editor_property("surface", getattr(ue.OpenFairwaySurface, enum_name))
        materials[name] = save(physical)
    return materials


def build_stripes():
    function = material_function(FOLDER, "MF_MowStripes",
                                 "Mowing-band mask (0 dark, 1 light) across a mow direction, filtered against moire.")
    position = function_input(function, "WorldPosition", ue.FunctionInputType.FUNCTION_INPUT_VECTOR3, (0, 0, 0, 0), 0,
                              "Defaults to the pixel's world position.")
    connect(node(function, ue.MaterialExpressionWorldPosition, "WorldPositionDefault"), position, "Preview")
    angle = function_input(function, "StripeAngleDeg", ue.FunctionInputType.FUNCTION_INPUT_SCALAR, (0, 0, 0, 0), 1,
                           "Mow direction, degrees from world +X (the hole line).")
    origin = function_input(function, "StripeOriginCm", ue.FunctionInputType.FUNCTION_INPUT_VECTOR2, (0, 0, 0, 0), 2,
                            "A point on the center line of a light band.")
    width = function_input(function, "StripeWidthCm", ue.FunctionInputType.FUNCTION_INPUT_SCALAR, (STRIPE_WIDTH_CM, 0, 0, 0), 3,
                           "Width of one band.")
    contrast = function_input(function, "StripeContrast", ue.FunctionInputType.FUNCTION_INPUT_SCALAR, (1, 0, 0, 0), 4,
                              "0 hides the bands, 1 is full dark-to-light.")
    mask = custom(function, "MowStripes", STRIPE_CODE,
                  {"P": (position, ""), "AngleDeg": (angle, ""), "Origin": (origin, ""), "WidthCm": (width, ""),
                   "Contrast": (contrast, "")}, ue.CustomMaterialOutputType.CMOT_FLOAT1)
    function_output(function, "Stripe", mask, 0)
    return finish_function(function)


def stripe_parameters(owner, stripes, group="Mowing"):
    call = function_call(owner, stripes, "MowStripes")
    origin = vector(owner, "StripeOriginCm", (0, 0, 0, 0), group, 1)
    origin_xy = node(owner, ue.MaterialExpressionComponentMask, "StripeOriginXY", r=True, g=True, b=False, a=False)
    connect(origin, origin_xy, "")
    connect(scalar(owner, "StripeAngleDeg", 0.0, group, 0), call, "StripeAngleDeg")
    connect(origin_xy, call, "StripeOriginCm")
    connect(scalar(owner, "StripeWidthCm", STRIPE_WIDTH_CM, group, 2), call, "StripeWidthCm")
    connect(scalar(owner, "StripeContrast", STRIPE_CONTRAST, group, 3), call, "StripeContrast")
    return call


def build_turf(stripes, textures):
    turf = material(FOLDER, "M_Turf")
    # The textures' normals already face up, so they are used directly as world normals on flat ground.
    turf.set_editor_property("tangent_space_normal", False)
    position = node(turf, ue.MaterialExpressionWorldPosition, "WorldPosition")
    size = scalar(turf, "TextureSizeCm", TEXTURE_SIZE_CM, "Texture", 0)
    uv = custom(turf, "WorldUV", "return P.xy / max(SizeCm, 1.0);", {"P": (position, ""), "SizeCm": (size, "")},
                ue.CustomMaterialOutputType.CMOT_FLOAT2)
    samples = {}
    for slot, (texture, sampler) in textures.items():
        sample = node(turf, ue.MaterialExpressionTextureSampleParameter2D, f"Param.Turf{slot}", parameter_name=f"Turf{slot}",
                      texture=texture, sampler_type=sampler, group="Texture")
        connect(uv, sample, "UVs")
        samples[slot] = sample
    palette = custom(turf, "TurfPalette",
                     "float detail = (saturate(dot(C, float3(0.2126, 0.7152, 0.0722)) * 2.0) - 0.5) * Detail;\n"
                     "return lerp(DarkGreen, LightGreen, saturate(Stripe + detail));",
                     {"Stripe": (stripe_parameters(turf, stripes), "Stripe"), "C": (samples["Color"], "RGB"),
                      "Detail": (scalar(turf, "DetailStrength", 0.12, "Grass appearance", 2), ""),
                      "DarkGreen": (vector(turf, "GrassDarkColor", linear_color(FAIRWAY_DARK_HEX), "Grass appearance", 0), "RGB"),
                      "LightGreen": (vector(turf, "GrassLightColor", linear_color(FAIRWAY_LIGHT_HEX), "Grass appearance", 1), "RGB")},
                     ue.CustomMaterialOutputType.CMOT_FLOAT3)
    output(turf, palette, ue.MaterialProperty.MP_BASE_COLOR)
    output(turf, samples["Normal"], ue.MaterialProperty.MP_NORMAL, "RGB")
    output(turf, samples["Roughness"], ue.MaterialProperty.MP_ROUGHNESS, "R")
    return finish_material(turf)


def build_surface_instances(turf, physical):
    fairway = material_instance(FOLDER, "MI_Fairway", turf)
    rough = material_instance(FOLDER, "MI_Rough", turf)
    green = material_instance(FOLDER, "MI_Green", turf)
    # Start from the parent's values, so a parameter dropped here does not linger as an override.
    for instance in (fairway, rough, green):
        MAT.clear_all_material_instance_parameters(instance)
    for instance, scalars, colors in (
            (rough, (("StripeContrast", 0.0), ("DetailStrength", 0.6)), (ROUGH_DARK_HEX, ROUGH_LIGHT_HEX)),
            (green, (("StripeContrast", 0.0), ("TextureSizeCm", GREEN_TEXTURE_SIZE_CM), ("DetailStrength", GREEN_DETAIL)),
             (GREEN_DARK_HEX, GREEN_LIGHT_HEX))):
        for parameter, value in scalars:
            MAT.set_material_instance_scalar_parameter_value(instance, parameter, value)
        for parameter, hex_rgb in zip(("GrassDarkColor", "GrassLightColor"), colors):
            MAT.set_material_instance_vector_parameter_value(instance, parameter, ue.LinearColor(*linear_color(hex_rgb)))
    for instance, surface in ((fairway, "Fairway"), (rough, "Rough"), (green, "Green")):
        if surface in physical:
            # Without the override flag GetPhysicalMaterial ignores PhysMaterial and returns the parent's.
            instance.set_editor_property("override_phys_material", True)
            instance.set_editor_property("phys_material", physical[surface])
        MAT.update_material_instance(instance)
        save(instance)
    return fairway, rough, green


def build_grass_blades(stripes):
    blades = material(GRASS_FOLDER, "M_GrassBlades")
    blades.set_editor_property("two_sided", True)
    blades.set_editor_property("used_with_instanced_static_meshes", True)
    # Both blade sides use upward world normals, matching the turf's lighting; world normals
    # avoid the back-face flip used by tangent-space normals, which flickered under TAA.
    blades.set_editor_property("tangent_space_normal", False)
    variation = node(blades, ue.MaterialExpressionPerInstanceRandom, "InstanceRandom")
    palette = custom(blades, "BladePalette",
                     "float detail = (R - 0.5) * 0.08;\nreturn lerp(DarkGreen, LightGreen, saturate(Stripe + detail));",
                     {"Stripe": (stripe_parameters(blades, stripes), "Stripe"), "R": (variation, ""),
                      "DarkGreen": (vector(blades, "GrassDarkColor", linear_color(FAIRWAY_DARK_HEX), "Grass appearance", 0), "RGB"),
                      "LightGreen": (vector(blades, "GrassLightColor", linear_color(FAIRWAY_LIGHT_HEX), "Grass appearance", 1), "RGB")},
                     ue.CustomMaterialOutputType.CMOT_FLOAT3)
    output(blades, palette, ue.MaterialProperty.MP_BASE_COLOR)
    output(blades, node(blades, ue.MaterialExpressionConstant3Vector, "GrassUp", constant=ue.LinearColor(0, 0, 1, 1)),
           ue.MaterialProperty.MP_NORMAL)
    output(blades, node(blades, ue.MaterialExpressionConstant, "GrassRoughness", r=0.9), ue.MaterialProperty.MP_ROUGHNESS)
    return finish_material(blades)


def import_textures():
    textures = {}
    for slot, suffix, sampler in (("Color", "Color", ue.MaterialSamplerType.SAMPLERTYPE_COLOR),
                                  ("Normal", "NormalDX", ue.MaterialSamplerType.SAMPLERTYPE_NORMAL),
                                  ("Roughness", "Roughness", ue.MaterialSamplerType.SAMPLERTYPE_MASKS)):
        texture = import_asset(f"Grass004_2K-JPG_{suffix}.jpg", FOLDER, f"T_Grass_{suffix}")
        texture.set_editor_property("srgb", slot == "Color")
        if slot == "Normal":
            texture.set_editor_property("compression_settings", ue.TextureCompressionSettings.TC_NORMALMAP)
            texture.set_editor_property("flip_green_channel", False)
        elif slot == "Roughness":
            texture.set_editor_property("compression_settings", ue.TextureCompressionSettings.TC_MASKS)
        textures[slot] = (save(texture), sampler)
    return textures


def build_surfaces():
    physical = build_physical_materials()
    stripes = build_stripes()
    turf = build_turf(stripes, import_textures())
    fairway, _, green = build_surface_instances(turf, physical)
    blades = build_grass_blades(stripes)
    clump = import_asset("SM_GrassClump.fbx", GRASS_FOLDER, "SM_GrassClump", mesh=True)
    clump.set_material(0, blades)
    save(clump)
    # The FBX carries its own convex hull (UCX_SM_GreenDisc_00); see PrepareCourseAssets.py.
    disc = import_asset("SM_GreenDisc.fbx", FOLDER, "SM_GreenDisc", mesh=True, collision=True)
    disc.set_material(0, green)
    hulls = ue.get_editor_subsystem(ue.StaticMeshEditorSubsystem).get_convex_collision_count(disc)
    if hulls != 1:
        raise RuntimeError(f"SM_GreenDisc imported {hulls} convex hulls; expected its one UCX_ hull")
    save(disc)
    surround = import_asset("SM_GreenSurround.fbx", FOLDER, "SM_GreenSurround", mesh=True, collision=True)
    surround.set_material(0, fairway)
    if ue.get_editor_subsystem(ue.StaticMeshEditorSubsystem).get_convex_collision_count(surround) != 64:
        raise RuntimeError("Green surround must retain its 64 separate collision wedges")
    save(surround)
    if MAT.get_material_instance_scalar_parameter_value(green, "StripeContrast") != 0.0:
        raise RuntimeError("The putting green still has mowing stripes")
    ue.log(f"Surfaces built: {len(physical)} physical materials, M_Turf, MI_Fairway, MI_Rough, MI_Green, M_GrassBlades, SM_GreenDisc")
