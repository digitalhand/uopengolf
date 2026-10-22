# SPDX-License-Identifier: MIT
"""Build open-licence course surface materials and presets. Applying them to a course is a separate step.

    py "<project>/Plugins/OpenGolfSavanna/Scripts/BuildSurfaceTextures.py" [fairway green bunker]

Run FetchAmbientCG.py first. Every surface shares one parent, /OpenGolfSavanna/Materials/M_SavannaSurface,
built from the course kit's surface graph plus a roughness map, a normal fade with distance and a
MacroStrength control. The kit's own turf parent is never changed. Each surface gets an instance in
Materials/<Category> and a surface preset in Terrain, keeping its physical material, so play is unchanged:
  fairway  Grass008 with the coastal fairway palette and mowing stripes
  green    Grass008 finer, lighter and without stripes or patchy macro variation
  bunker   Ground093C sand
"""
from pathlib import Path
import sys
import unreal as ue

PROJECT = Path(ue.Paths.project_dir()).resolve()
sys.path.insert(0, str(PROJECT / 'Plugins/OpenGolfCourse/Scripts'))
import course_assets as a
import BuildTerrainAssets as terrain

MOUNT = '/OpenGolfSavanna'
OWNER = 'OpenGolfSavanna.Surfaces.v1'
PARENT = 'M_SavannaSurface'
TEXTURE_ROLES = {'Color': 'TurfTexture', 'NormalDX': 'TurfNormal', 'Roughness': 'RoughnessTexture'}
# Mean sRGB colour of each pinned 2K Color map (averaged in linear space). The material scales the
# texture by TargetColor / TextureMean, so its average lands on the target and its detail is kept.
TEXTURE_MEANS = {'Grass008': '66852D', 'Ground093C': 'CBB893'}
# Starting looks; tune the instances after review. Colours are sRGB hex. DetailStrength blends from
# the flat target colour (0) to the full texture (1); MacroStrength is the broad brightness variation.
SURFACES = {
    'fairway': dict(asset='Grass008', surface='FAIRWAY', physical='PM_Fairway', category='Fairway',
                    scalars={'TextureSizeCm': 110, 'NormalStrength': 0.12, 'DetailStrength': 0.8, 'StripeContrast': 0.09,
                             'Roughness': 0.9, 'Specular': 0.15, 'MacroStrength': 0.2},
                    colors={'TargetColor': '5F7F3C'}),
    # About 15% lighter than the fairway; no stripes (project rule) and little macro variation, so no patches.
    'green': dict(asset='Grass008', surface='GREEN', physical='PM_Green', category='Green',
                  scalars={'TextureSizeCm': 45, 'NormalStrength': 0.05, 'DetailStrength': 0.6, 'StripeContrast': 0,
                           'Roughness': 0.92, 'Specular': 0.15, 'MacroStrength': 0.06},
                  colors={'TargetColor': '6E9148'}),
    # Sand keeps its own colour; most of its detail is in the normal map.
    'bunker': dict(asset='Ground093C', surface='SAND', physical='PM_Sand', category='Bunker',
                   scalars={'TextureSizeCm': 150, 'NormalStrength': 0.3, 'DetailStrength': 1.0, 'StripeContrast': 0,
                            'Roughness': 0.85, 'Specular': 0.2, 'MacroStrength': 0.12},
                   colors={'TargetColor': 'CBB893'}),
}


def build_parent(textures):
    terrain.FOLDER = 'Materials'
    parent = terrain.build_material(PARENT)
    # Fine normal detail fades with distance, so far turf does not shimmer.
    depth = a.node(parent, ue.MaterialExpressionPixelDepth, 'SurfaceDepth')
    strength = a.scalar(parent, 'NormalStrength', 0.12, 'Surface')
    faded = a.custom(parent, 'SurfaceNormalFade', 'return S*(1-smoothstep(500.0,2500.0,D));',
                     {'S': (strength, ''), 'D': (depth, '')}, ue.CustomMaterialOutputType.CMOT_FLOAT1)
    a.connect(faded, a.node(parent, ue.MaterialExpressionCustom, 'NormalStrength'), 'Strength')
    # Unlike the kit's palette graph, which keeps only the texture's luminance, the photo texture
    # carries the colour detail: it is scaled so its mean lands on TargetColor. The macro
    # variation is exposed so greens can stay even.
    color = a.node(parent, ue.MaterialExpressionTextureSampleParameter2D, 'TurfTexture')
    position = a.node(parent, ue.MaterialExpressionWorldPosition, 'Position')
    palette = a.custom(parent, 'SurfaceColor',
                       'float3 tex=C*T/max(M,0.001);\n'
                       'float stripe=0.5+0.5*sin(P.y/400.0*3.14159265);\n'
                       'float band=Contrast*(stripe-0.5)*(1-smoothstep(40.0,200.0,fwidth(P.y)));\n'
                       'return lerp(T,tex,Detail)*(1+(Macro-0.5)*MacroStrength+band);',
                       {'C': (color, 'RGB'), 'P': (position, ''),
                        'T': (a.vector(parent, 'TargetColor', a.linear_color('5F7F3C'), 'Surface'), 'RGB'),
                        'M': (a.vector(parent, 'TextureMean', a.linear_color('66852D'), 'Surface'), 'RGB'),
                        'Macro': (a.node(parent, ue.MaterialExpressionCustom, 'MacroVariation'), ''),
                        'MacroStrength': (a.scalar(parent, 'MacroStrength', 0.35, 'Surface'), ''),
                        'Detail': (a.scalar(parent, 'DetailStrength', 0.5, 'Surface'), ''),
                        'Contrast': (a.scalar(parent, 'StripeContrast', 0, 'Surface'), '')},
                       ue.CustomMaterialOutputType.CMOT_FLOAT3)
    a.output(parent, palette, ue.MaterialProperty.MP_BASE_COLOR)
    rough = a.node(parent, ue.MaterialExpressionTextureSampleParameter2D, 'SurfaceRoughnessMap',
                   parameter_name='RoughnessTexture', texture=textures['Roughness'],
                   sampler_type=ue.MaterialSamplerType.SAMPLERTYPE_MASKS)
    a.connect(a.node(parent, ue.MaterialExpressionCustom, 'UV'), rough, 'UVs')
    floor = a.scalar(parent, 'Roughness', 0.9, 'Surface')
    remap = a.custom(parent, 'SurfaceRoughness', 'return max(F,R);',
                     {'F': (floor, ''), 'R': (rough, 'R')}, ue.CustomMaterialOutputType.CMOT_FLOAT1)
    a.output(parent, remap, ue.MaterialProperty.MP_ROUGHNESS)
    return a.finish_material(parent)


def import_textures(asset):
    a.SOURCE = PROJECT / 'Saved/AssetImport' / asset
    if not (a.SOURCE / f'{asset}_2K-JPG_Color.jpg').exists():
        raise RuntimeError(f'Run FetchAmbientCG.py {asset} first: {a.SOURCE} has no maps')
    textures = {}
    for role in TEXTURE_ROLES:
        texture = a.import_asset(f'{asset}_2K-JPG_{role}.jpg', f'Textures/Surfaces/{asset}', f'T_{asset}_{role}')
        texture.set_editor_property('srgb', role == 'Color')
        if role == 'NormalDX':
            texture.set_editor_property('compression_settings', ue.TextureCompressionSettings.TC_NORMALMAP)
        elif role == 'Roughness':
            texture.set_editor_property('compression_settings', ue.TextureCompressionSettings.TC_MASKS)
        textures[role] = a.save(texture)
    return textures


def build_surface(name, spec, parent, textures):
    folder = f"Materials/{spec['category']}"
    instance = a.material_instance(folder, f"MI_{spec['category']}{spec['asset']}", parent)
    for role, parameter in TEXTURE_ROLES.items():
        a.MAT.set_material_instance_texture_parameter_value(instance, parameter, textures[role])
    for key, value in spec['scalars'].items():
        a.MAT.set_material_instance_scalar_parameter_value(instance, key, value)
    colors = dict(spec['colors'], TextureMean=TEXTURE_MEANS[spec['asset']])
    for key, color in colors.items():
        a.MAT.set_material_instance_vector_parameter_value(instance, key, ue.LinearColor(*a.linear_color(color)))
    # The physical material carries the surface's play; only the look changes.
    instance.set_editor_property('override_phys_material', True)
    instance.set_editor_property('phys_material', ue.load_asset(f"/OpenGolfCourse/Surfaces/{spec['physical']}"))
    a.MAT.update_material_instance(instance)
    a.save(instance)
    preset = terrain.create_data(f"DA_{spec['category']}{spec['asset']}", ue.OpenGolfSurfacePreset, 'Terrain')
    preset.set_editor_property('surface', getattr(ue.OpenFairwaySurface, spec['surface']))
    preset.set_editor_property('material', instance)
    a.save(preset)
    ue.log(f"OPEN_GOLF_SURFACE {name} {preset.get_path_name()}")
    return preset


def build(names=tuple(SURFACES)):
    unknown = set(names) - set(SURFACES)
    if unknown:
        raise RuntimeError(f'Unknown surfaces {sorted(unknown)}; choose from {sorted(SURFACES)}')
    previous = (a.MOUNT, a.OWNER, a.CONTENT_DIR, a.SOURCE, terrain.FOLDER)
    try:
        a.MOUNT, a.OWNER = MOUNT, OWNER
        a.CONTENT_DIR = PROJECT / 'Plugins/OpenGolfSavanna/Content'
        textures = {asset: import_textures(asset) for asset in sorted({SURFACES[n]['asset'] for n in names})}
        # The parent's default roughness map only fills the parameter; every instance sets its own.
        parent = build_parent(next(iter(textures.values())))
        presets = [build_surface(name, SURFACES[name], parent, textures[SURFACES[name]['asset']]) for name in names]
        ue.log(f'OPEN_GOLF_SURFACES_COMPLETE {len(presets)}')
        return presets
    finally:
        a.MOUNT, a.OWNER, a.CONTENT_DIR, a.SOURCE, terrain.FOLDER = previous


if __name__ == '__main__':
    build(tuple(sys.argv[1:]) or tuple(SURFACES))
