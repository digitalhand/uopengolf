# SPDX-License-Identifier: MIT
"""The rough grass parent material, M_SavannaGrass, for Poly Haven grass tufts.

Masked two-sided blades (both Poly Haven grasses cut blades from cards with an alpha map), Nanite.
World Position Offset is the course kit's vegetation wind, off until an instance turns it on. The grass stability rules from the course kit apply: shading uses mostly
upward world normals, with the tuft's normal detail only up close, and roughness stays high.

Colour blends from the green diffuse to a dry one (Grass Medium's dry_diff, or the green one times
DryTint) by distance from play: a course mask (white near play) is sampled in terrain metres. The
mask texture and its bounds are instance parameters, so one parent serves any range layout;
the defaults (white mask) keep the grass green. Dryness also drifts with world-space patch noise.
Nothing varies per instance: the rough is tiled squares of tufts, and a per-instance colour shows
each square.
"""
import unreal as ue

# Not reloaded here: reloading savanna_assets reloads course_assets, which would drop the mount that
# savanna_helpers() set and send new assets to /OpenGolfCourse. Callers reload savanna_assets first.
import savanna_assets as s
import BuildTerrainAssets as terrain  # After savanna_assets, which puts the course kit's scripts on sys.path.
import turf_palette as palette

a = s.a


def course_mask_uv(material, position):
    """UV of a world position in the course mask, from the terrain frame and mask bounds (metres)."""
    origin = a.vector(material, "MaskOriginCm", (0, 0, 0, 0), "Course mask", 0)
    forward = a.vector(material, "MaskForward", (1, 0, 0, 0), "Course mask", 1)
    right = a.vector(material, "MaskRight", (0, 1, 0, 0), "Course mask", 2)
    low = a.vector(material, "MaskMinM", (-1, -1, 0, 0), "Course mask", 3)
    high = a.vector(material, "MaskMaxM", (1, 1, 0, 0), "Course mask", 4)
    return a.custom(material, "CourseMaskUV",
                    "float3 q=P-O;"
                    "float2 m=float2(dot(q.xy,F.xy),dot(q.xy,R.xy))/100.0;"
                    "return (m-L.xy)/max(H.xy-L.xy,0.001);",
                    {"P": (position, ""), "O": (origin, "RGB"), "F": (forward, "RGB"), "R": (right, "RGB"),
                     "L": (low, "RGB"), "H": (high, "RGB")},
                    ue.CustomMaterialOutputType.CMOT_FLOAT2)


def grass_material():
    material = a.material("Materials", "M_SavannaGrass")
    for key, value in (("two_sided", True), ("tangent_space_normal", False), ("used_with_instanced_static_meshes", True),
                       ("used_with_nanite", True), ("blend_mode", ue.BlendMode.BLEND_MASKED), ("opacity_mask_clip_value", 0.4)):
        material.set_editor_property(key, value)
    uv = a.node(material, ue.MaterialExpressionTextureCoordinate, "TuftUV")
    albedo = s.sample(material, "Albedo", s.WHITE, uv)
    dry = s.sample(material, "AlbedoDry", s.WHITE, uv)
    opacity = s.sample(material, "Opacity", s.WHITE_MASK, uv, mask=True)
    normal = s.sample(material, "Normal", s.FLAT_NORMAL, uv, normal=True)
    rough = s.sample(material, "Roughness", s.WHITE_MASK, uv, mask=True)
    ao = s.sample(material, "AmbientOcclusion", s.WHITE_MASK, uv, mask=True)
    position = a.node(material, ue.MaterialExpressionWorldPosition, "WorldPosition")
    mask = s.sample(material, "CourseMask", s.WHITE_MASK, course_mask_uv(material, position), mask=True)
    octaves = {pin: (a.custom(material, "GrassPatchNoise" + pin, terrain.value_noise(cell, seed=21 + i),
                              {"P": (position, "")}, ue.CustomMaterialOutputType.CMOT_FLOAT1), "")
               for i, (pin, cell) in enumerate(zip(("A", "B"), palette.GRASS_CELLS_CM))}
    patches = a.custom(material, "GrassPatches", palette.GRASS_PATCH_HLSL, octaves,
                       ue.CustomMaterialOutputType.CMOT_FLOAT1)
    color = a.custom(material, "GrassColor", palette.GRASS_COLOR_HLSL,
                     {"A": (albedo, "RGB"), "D": (dry, "RGB"), "M": (mask, "R"), "Noise": (patches, ""),
                      "Near": (a.scalar(material, "DryNearPlay", 0.1, "Grass", 0), ""),
                      "Far": (a.scalar(material, "DryFarFromPlay", 0.8, "Grass", 1), ""),
                      "Jit": (a.scalar(material, "DryJitter", 0.3, "Grass", 2), ""),
                      "DT": (a.vector(material, "DryTint", (1, 1, 1, 1), "Grass", 3), "RGB"),
                      "T": (a.vector(material, "Tint", (1, 1, 1, 1), "Grass", 4), "RGB"),
                      "O": (ao, "R"), "S": (a.scalar(material, "AOStrength", 0.5, "Grass", 5), ""),
                      "B": (a.scalar(material, "Brightness", 1.0, "Grass", 6), ""),
                      "Sat": (a.scalar(material, "Saturation", 1.0, "Grass", 7), "")},
                     ue.CustomMaterialOutputType.CMOT_FLOAT3)
    world_normal = a.node(material, ue.MaterialExpressionTransform, "NormalToWorld",
                          transform_source_type=ue.MaterialVectorCoordTransformSource.TRANSFORMSOURCE_TANGENT,
                          transform_type=ue.MaterialVectorCoordTransform.TRANSFORM_WORLD)
    a.connect(normal, world_normal, "", "RGB")
    depth = a.node(material, ue.MaterialExpressionPixelDepth, "PixelDepth")
    shading = a.custom(material, "GrassNormal",
                       "float fade=saturate(1-(D-F0)/max(F1-F0,1));"
                       "return normalize(lerp(float3(0,0,1),N,S*fade));",
                       {"N": (world_normal, ""), "D": (depth, ""),
                        "S": (a.scalar(material, "NormalStrength", 0.35, "Shading", 0), ""),
                        "F0": (a.scalar(material, "NormalFadeStartCm", 500, "Shading", 1), ""),
                        "F1": (a.scalar(material, "NormalFadeEndCm", 2000, "Shading", 2), "")},
                       ue.CustomMaterialOutputType.CMOT_FLOAT3)
    roughness = a.custom(material, "GrassRoughness", "return lerp(L,1,R);",
                         {"R": (rough, "G"), "L": (a.scalar(material, "RoughnessMin", 0.9, "Shading", 3), "")},
                         ue.CustomMaterialOutputType.CMOT_FLOAT1)
    a.output(material, color, ue.MaterialProperty.MP_BASE_COLOR)
    a.output(material, shading, ue.MaterialProperty.MP_NORMAL)
    a.output(material, opacity, ue.MaterialProperty.MP_OPACITY_MASK, "R")
    a.output(material, roughness, ue.MaterialProperty.MP_ROUGHNESS)
    a.output(material, a.scalar(material, "Specular", 0.15, "Shading", 4), ue.MaterialProperty.MP_SPECULAR)
    s.wind.add_vegetation_wind(material)
    return a.finish_material(material)
