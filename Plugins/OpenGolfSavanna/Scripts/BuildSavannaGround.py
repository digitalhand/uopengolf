# SPDX-License-Identifier: MIT
"""Build the open-licence desert ground parent, /OpenGolfSavanna/Materials/M_SavannaGround.

    py "<project>/Plugins/OpenGolfSavanna/Scripts/BuildSavannaGround.py"

Fetch the sources first:
    python3 Plugins/OpenGolfSavanna/Scripts/FetchPolyHaven.py red_sand leafy_grass
    python3 Plugins/OpenGolfSavanna/Scripts/FetchAmbientCG.py Rock029

The ground is a sand base with Rock029 patches (ambientCG, CC0), and near play the rough
underlay (Poly Haven leafy_grass, CC0) through a per-course mask texture. Each texture is
scaled so its mean lands on a target colour, so a sand can be swapped without retuning the
rest. Instances choose the sand (SAND_TEXTURES) and set the course mask and its bounds;
NativeOnly=1 drops the rough for ground outside the course. Parameter names match the
earlier ground material, so instances can be reparented in place.
"""
from pathlib import Path
import importlib
import sys

import unreal as ue

PROJECT = Path(ue.Paths.project_dir()).resolve()
HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(PROJECT / 'Plugins/OpenGolfCourse/Scripts'))
sys.path.insert(0, str(HERE))
import art_root
import BuildTerrainAssets as terrain
import savanna_assets as s

s = importlib.reload(s)
terrain = importlib.reload(terrain)
a = s.a
PARENT = 'M_SavannaGround'
POLYHAVEN = art_root.art_root() / 'PolyHaven'
ROCK_SOURCE = PROJECT / 'Saved/AssetImport/Rock029'
# Sands: textures and the mean sRGB colour of each 2K JPG colour map (averaged in linear space).
# red_sand was chosen over aerial_sand on 2026-10-21; aerial_sand's dune bumps repeated every
# 2.5 m. red_sand's detail is mostly in its normal map.
SANDS = {
    'red_sand': dict(folder='RedSand', mean='815A46'),
}
ROCK_MEAN = '7D5538'
LEAFY_GRASS = '/OpenGolfSavanna/Textures/Ground/LeafyGrass/T_leafy_grass_'
# Target colours for the rendered desert in the close view.
SAND_TARGET = '835D45'
ROCK_TARGET = '87583C'
LUMA = 'float3(0.2126,0.7152,0.0722)'


def import_sand(name):
    folder = f"Textures/Ground/{SANDS[name]['folder']}"
    maps = POLYHAVEN / name / 'maps'
    if not (maps / f'{name}_diff_2k.jpg').exists():
        raise RuntimeError(f'Run FetchPolyHaven.py {name} first: {maps} has no maps')
    normal = s.import_texture(str(maps / f'{name}_nor_gl_2k.jpg'), folder, 'normal')
    return {'SandColor': s.import_texture(str(maps / f'{name}_diff_2k.jpg'), folder, 'albedo'),
            'SandNormal': normal,
            'SandRoughness': s.import_texture(str(maps / f'{name}_rough_2k.jpg'), folder, 'roughness', normal)}


def import_rock():
    if not (ROCK_SOURCE / 'Rock029_2K-JPG_Color.jpg').exists():
        raise RuntimeError(f'Run FetchAmbientCG.py Rock029 first: {ROCK_SOURCE} has no maps')
    folder = 'Textures/Ground/Rock029'
    return {'RockColor': s.import_texture(str(ROCK_SOURCE / 'Rock029_2K-JPG_Color.jpg'), folder, 'albedo'),
            'RockNormal': s.import_texture(str(ROCK_SOURCE / 'Rock029_2K-JPG_NormalDX.jpg'), folder, 'normal',
                                           opengl_normals=False)}


def sample(material, name, texture, uv, kind):
    node = a.node(material, ue.MaterialExpressionTextureSampleParameter2D, name, parameter_name=name,
                  texture=texture, sampler_type=kind)
    a.connect(uv, node, 'UVs')
    return node


def tiling(material, name, position, size_name, size_cm):
    size = a.scalar(material, size_name, size_cm, 'Ground')
    return a.custom(material, name, 'return P.xy/max(S,1.0);', {'P': (position, ''), 'S': (size, '')},
                    ue.CustomMaterialOutputType.CMOT_FLOAT2)


def build_parent(sand, rock):
    material = a.material('Materials', PARENT)
    COLOR, NORMAL = ue.MaterialSamplerType.SAMPLERTYPE_COLOR, ue.MaterialSamplerType.SAMPLERTYPE_NORMAL
    MASKS = ue.MaterialSamplerType.SAMPLERTYPE_MASKS
    world = a.node(material, ue.MaterialExpressionWorldPosition, 'WorldPosition')
    meters = a.node(material, ue.MaterialExpressionTextureCoordinate, 'TerrainMeters')
    depth = a.node(material, ue.MaterialExpressionPixelDepth, 'GroundDepth')
    sand_uv = tiling(material, 'SandUV', world, 'SandSizeCm', 250.0)
    rock_uv = tiling(material, 'RockUV', world, 'RockSizeCm', 600.0)
    grass_uv = tiling(material, 'GrassUV', world, 'GrassSizeCm', 180.0)
    # The course mask covers the terrain's detail bounds: (minimum x, minimum y, size x, size y) in metres.
    bounds = a.vector(material, 'MaskBounds', (0.0, 0.0, 1.0, 1.0), 'Course')
    mask_uv = a.custom(material, 'CourseMaskUV', 'return (M.xy-B.xy)/max(B.zw,0.001);',
                       {'M': (meters, ''), 'B': (bounds, 'RGBA')}, ue.CustomMaterialOutputType.CMOT_FLOAT2)
    mask = sample(material, 'CourseMask', s.required('/OpenGolfSavanna/Materials/T_WhiteMask'), mask_uv, MASKS)
    native = a.scalar(material, 'NativeOnly', 0, 'Course')
    rough_weight = a.custom(material, 'RoughWeight', 'return M*(1-N);', {'M': (mask, 'R'), 'N': (native, '')},
                            ue.CustomMaterialOutputType.CMOT_FLOAT1)
    sand_color = sample(material, 'SandColor', sand['SandColor'], sand_uv, COLOR)
    sand_normal = sample(material, 'SandNormal', sand['SandNormal'], sand_uv, NORMAL)
    sand_rough = sample(material, 'SandRoughness', sand['SandRoughness'], sand_uv, MASKS)
    rock_color = sample(material, 'RockColor', rock['RockColor'], rock_uv, COLOR)
    rock_normal = sample(material, 'RockNormal', rock['RockNormal'], rock_uv, NORMAL)
    grass_color = sample(material, 'GrassColor', s.required(LEAFY_GRASS + 'diff_2k'), grass_uv, COLOR)
    grass_normal = sample(material, 'GrassNormal', s.required(LEAFY_GRASS + 'nor_gl_2k'), grass_uv, NORMAL)

    # Rock patches: two octaves of seam-free value noise, thresholded, and never in the rough.
    large = a.custom(material, 'RockNoiseLarge', terrain.value_noise(3100, seed=1), {'P': (world, '')},
                     ue.CustomMaterialOutputType.CMOT_FLOAT1)
    small = a.custom(material, 'RockNoiseSmall', terrain.value_noise(1100, seed=2), {'P': (world, '')},
                     ue.CustomMaterialOutputType.CMOT_FLOAT1)
    rock_weight = a.custom(material, 'RockWeight',
                           'float n=0.65*A+0.35*B; return saturate((n-(1-Amount))/max(Edge,0.001))*(1-W);',
                           {'A': (large, ''), 'B': (small, ''), 'W': (rough_weight, ''),
                            'Amount': (a.scalar(material, 'RockAmount', 0.3, 'Rock'), ''),
                            'Edge': (a.scalar(material, 'RockEdge', 0.08, 'Rock'), '')},
                           ue.CustomMaterialOutputType.CMOT_FLOAT1)
    color = a.custom(material, 'GroundColor',
                     'float macro=0.5+0.5*sin(P.x/2300.0)*sin(P.y/1700.0);\n'
                     'float3 sand=S*ST/max(SM,0.001);\n'
                     'float3 stone=R*RT/max(RM,0.001);\n'
                     'float3 dry=lerp(sand,stone,Rock)*(0.92+macro*0.16);\n'
                     f'float3 turf=lerp(L,H,saturate(0.25+dot(G,{LUMA})*0.65+macro*0.15));\n'
                     'return lerp(dry,turf,W);',
                     {'P': (world, ''), 'S': (sand_color, 'RGB'), 'R': (rock_color, 'RGB'), 'G': (grass_color, 'RGB'),
                      'ST': (a.vector(material, 'SandTarget', a.linear_color(SAND_TARGET), 'Sand'), 'RGB'),
                      'SM': (a.vector(material, 'SandMean', a.linear_color(SANDS['red_sand']['mean']), 'Sand'), 'RGB'),
                      'RT': (a.vector(material, 'RockTarget', a.linear_color(ROCK_TARGET), 'Rock'), 'RGB'),
                      'RM': (a.vector(material, 'RockMean', a.linear_color(ROCK_MEAN), 'Rock'), 'RGB'),
                      'L': (a.vector(material, 'RoughLow', a.linear_color('53653B'), 'Rough'), 'RGB'),
                      'H': (a.vector(material, 'RoughHigh', a.linear_color('7B8952'), 'Rough'), 'RGB'),
                      'Rock': (rock_weight, ''), 'W': (rough_weight, '')},
                     ue.CustomMaterialOutputType.CMOT_FLOAT3)
    # Normal detail fades out with distance, so the open desert stays stable in motion.
    normal = a.custom(material, 'GroundNormal',
                      'float fade=1-smoothstep(2000.0,8000.0,D);\n'
                      'float3 up=float3(0,0,1);\n'
                      'float3 dry=lerp(lerp(up,SN,SS*fade),lerp(up,RN,RS*fade),Rock);\n'
                      'return normalize(lerp(dry,lerp(up,GN,GS*fade),W));',
                      {'D': (depth, ''), 'SN': (sand_normal, 'RGB'), 'RN': (rock_normal, 'RGB'), 'GN': (grass_normal, 'RGB'),
                       'SS': (a.scalar(material, 'SandNormalStrength', 0.3, 'Sand'), ''),
                       'RS': (a.scalar(material, 'RockNormalStrength', 0.5, 'Rock'), ''),
                       'GS': (a.scalar(material, 'GrassNormalStrength', 0.15, 'Rough'), ''),
                       'Rock': (rock_weight, ''), 'W': (rough_weight, '')},
                      ue.CustomMaterialOutputType.CMOT_FLOAT3)
    roughness = a.custom(material, 'GroundRoughness', 'return max(F,lerp(SR,0.9,saturate(Rock+W)));',
                         {'F': (a.scalar(material, 'Roughness', 0.9, 'Ground'), ''), 'SR': (sand_rough, 'G'),
                          'Rock': (rock_weight, ''), 'W': (rough_weight, '')},
                         ue.CustomMaterialOutputType.CMOT_FLOAT1)
    a.output(material, color, ue.MaterialProperty.MP_BASE_COLOR)
    a.output(material, normal, ue.MaterialProperty.MP_NORMAL)
    a.output(material, roughness, ue.MaterialProperty.MP_ROUGHNESS)
    a.output(material, a.scalar(material, 'Specular', 0.08, 'Ground'), ue.MaterialProperty.MP_SPECULAR)
    return a.finish_material(material)


def build():
    with s.savanna_helpers():
        sands = {name: import_sand(name) for name in SANDS}
        rock = import_rock()
        parent = build_parent(sands['red_sand'], rock)
    ue.log(f'OPEN_GOLF_SAVANNA_GROUND_COMPLETE {parent.get_path_name()} sands={sorted(sands)}')
    return parent, sands


# Ground under the Grass Medium tiles: Leafy Grass tinted to the shaded base of the blades (their
# albedo averages 60632A), so gaps between tiles and the cull fade read as grass. Linear colours,
# about half the blades' albedo: 3D blades render darker than flat ground under the same light.
ROUGH_UNDERLAY_COLORS = {"RoughLow": (0.012, 0.024, 0.005, 1.0), "RoughHigh": (0.035, 0.06, 0.015, 1.0)}
DEFAULT_SAND = 'red_sand'


def course_mask(definition, folder, name):
    """Write and import a course's rough mask (white near play) into folder/name of the current helpers.

    Returns the texture and the definition's detail bounds, which the mask covers.
    """
    import savanna_layout
    import struct
    savanna_layout = importlib.reload(savanna_layout)
    minimum = definition.get_editor_property('detail_minimum_m')
    maximum = definition.get_editor_property('detail_maximum_m')
    outlines = [[(p.x, p.y) for p in region.outline_m] for region in definition.regions
                if region.surface and region.surface.surface != ue.OpenFairwaySurface.ROUGH]
    width, height = 1024, 512
    values = savanna_layout.ground_mask(outlines, (minimum.x, minimum.y), (maximum.x, maximum.y), width, height)
    source = PROJECT / 'Saved/AssetImport/CourseMasks' / f'{name}.tga'
    source.parent.mkdir(parents=True, exist_ok=True)
    header = struct.pack('<BBBHHBHHHHBB', 0, 0, 2, 0, 0, 0, 0, 0, width, height, 24, 0x20)
    source.write_bytes(header + b''.join(bytes((v, v, v)) for v in values))
    texture = a.import_asset(str(source), folder, name)
    texture.set_editor_property('srgb', False)
    texture.set_editor_property('compression_settings', ue.TextureCompressionSettings.TC_MASKS)
    texture.set_editor_property('mip_gen_settings', ue.TextureMipGenSettings.TMGS_SIMPLE_AVERAGE)
    texture.set_editor_property('address_x', ue.TextureAddress.TA_CLAMP)
    texture.set_editor_property('address_y', ue.TextureAddress.TA_CLAMP)
    return a.save(texture), minimum, maximum


def apply_rough_underlay(instance):
    """Point a ground instance's rough at Leafy Grass with the underlay colours."""
    MAT = ue.MaterialEditingLibrary
    MAT.set_material_instance_texture_parameter_value(instance, 'GrassColor', s.required(LEAFY_GRASS + 'diff_2k'))
    MAT.set_material_instance_texture_parameter_value(instance, 'GrassNormal', s.required(LEAFY_GRASS + 'nor_gl_2k'))
    for name, rgba in ROUGH_UNDERLAY_COLORS.items():
        MAT.set_material_instance_vector_parameter_value(instance, name, ue.LinearColor(*rgba))
    MAT.update_material_instance(instance)
    return instance


def configure_ground(instance, native, mask, minimum, maximum, sand=DEFAULT_SAND):
    """Point a ground instance at M_SavannaGround with a course's mask and bounds.

    Every earlier override is cleared first, so references to an earlier parent's textures don't survive.
    """
    MAT = ue.MaterialEditingLibrary
    MAT.set_material_instance_parent(instance, s.required(f'/OpenGolfSavanna/Materials/{PARENT}'))
    MAT.clear_all_material_instance_parameters(instance)
    textures, colors = sand_settings(sand)
    for name, path in textures.items():
        MAT.set_material_instance_texture_parameter_value(instance, name, s.required(path))
    for name, color in colors.items():
        MAT.set_material_instance_vector_parameter_value(instance, name, ue.LinearColor(*a.linear_color(color)))
    MAT.set_material_instance_texture_parameter_value(instance, 'CourseMask', mask)
    bounds = ue.LinearColor(minimum.x, minimum.y, maximum.x - minimum.x, maximum.y - minimum.y)
    MAT.set_material_instance_vector_parameter_value(instance, 'MaskBounds', bounds)
    MAT.set_material_instance_scalar_parameter_value(instance, 'NativeOnly', native)
    if not native:
        apply_rough_underlay(instance)
    MAT.update_material_instance(instance)
    return instance


def sand_settings(name):
    """Texture and colour overrides that select a sand on an instance of the parent."""
    folder = f"/OpenGolfSavanna/Textures/Ground/{SANDS[name]['folder']}/T_{name}_"
    textures = {'SandColor': folder + 'diff_2k', 'SandNormal': folder + 'nor_gl_2k', 'SandRoughness': folder + 'rough_2k'}
    return textures, {'SandMean': SANDS[name]['mean'], 'SandTarget': SAND_TARGET}


if __name__ == '__main__':
    build()
