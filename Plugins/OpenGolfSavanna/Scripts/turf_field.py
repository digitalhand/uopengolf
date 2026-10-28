# SPDX-License-Identifier: MIT
"""One world-space turf field shared by the course ground and the cut-turf blades.

Every ground material and the blades evaluate the same function of world position, so a seam
between two ground materials shows no change of colour and a blade matches the ground under it.
Layers, lowest first: rough, first cut, fairway, bunker lip, collar, green, tee. Signed distances
come from T_CourseSurfaceField (turf_mask.py); the square tee box is analytic. Each family mixes
its mid tone with drier and lusher patch tones (turf_palette.py) by world-space noise.
"""
import json
from pathlib import Path

import unreal as ue
import course_assets as a
import BuildTerrainAssets as terrain
import turf_palette as palette

FAMILIES = ('Fairway', 'FirstCut', 'Collar', 'Green', 'Tee')
MAPS = {'Tee': 'Collar'}  # The tee is finely cut, so it shares the collar's maps.
SCALARS = ('TextureSizeCm', 'DetailStrength', 'MacroStrength', 'StripeContrast', 'StripePeriodCm', 'HueAmount')
VECTORS = ('TargetColor', 'DryColor', 'LushColor', 'TextureMean')
DEFAULTS = {'TextureSizeCm': 50, 'DetailStrength': 1.35, 'MacroStrength': .15, 'StripeContrast': 0, 'StripePeriodCm': 400,
            'HueAmount': 1}
FAMILY_DEFAULTS = {'Fairway': {'StripeContrast': .06}, 'Tee': {'StripeContrast': .04, 'StripePeriodCm': 100}}
EDGES = {'FirstCutWidthM': 1.0, 'MowEdgeM': .06, 'FirstCutOuterEdgeM': .2, 'EdgeWobbleM': .03,
         'LipWidthM': .25, 'LipShade': .8}
FIELD_PATH = '/OpenGolfSavanna/Textures/CutTurf/T_CourseSurfaceField'
PLACEHOLDER = '/OpenGolfSavanna/Materials/T_WhiteMask'
GROUP = 'Turf field'

# Signed distances in terrain metres (negative inside), then each layer's coverage. The tee box,
# fairway, collar and green edges are mowing lines; the first cut's outer edge is softer.
FIELD_HLSL = '''float4 d=(S-0.5)*2*R;
float2 q=(P.xy-O.xy)/100.0; float2 L=float2(dot(q,F.xy),dot(q,float2(-F.y,F.x)));
float n=(N-0.5)*Wob; d.xyz+=n;
float ef=max(Mow,fwidth(d.x)), eg=max(Mow,fwidth(d.y)), ec=max(Mow,fwidth(d.z));
float fairway=1-smoothstep(-ef,ef,d.x);
float eo=max(Outer,fwidth(d.x)); float cut=1-smoothstep(W-eo,W+eo,d.x);
float collar=1-smoothstep(-ec,ec,d.z);
float green=1-smoothstep(-eg,eg,d.y);
float lip=(1-smoothstep(0,max(LipW,0.001),d.w))*smoothstep(-0.02,0.02,d.w);
float2 t=abs(L-TeeBox.xy)-TeeBox.zw; float tb=length(max(t,0))+min(max(t.x,t.y),0);
float et=max(Mow,fwidth(tb)); float tee=(TeeBox.z>0&&TeeBox.w>0)?1-smoothstep(-et,et,tb):0;
'''
COLOR_HLSL = FIELD_HLSL + '''float3 c=lerp(Base,FirstCut,cut);
c=lerp(c,Fairway,fairway);
c*=lerp(1,LipShade,lip);
c=lerp(c,Collar,collar);
c=lerp(c,Green,green);
return lerp(c,Tee,tee);'''
COVERAGE_HLSL = FIELD_HLSL + 'return saturate(max(cut,tee));'


def field_texture():
    return ue.load_asset(FIELD_PATH) or ue.load_asset(PLACEHOLDER)


def family_color(material, family, position, hue, bright):
    """One family's turf colour: its tone mixed towards drier or lusher patches, the texture's grain around that tone,
    brightness variation and stripes."""
    def scalar(name):
        return (a.scalar(material, family + name, DEFAULTS[name], family), '')
    uv = a.custom(material, family + 'FieldUV', 'return P.xy/max(S,1.0);',
                  {'P': (position, ''), 'S': scalar('TextureSizeCm')}, ue.CustomMaterialOutputType.CMOT_FLOAT2)
    maps = MAPS.get(family, family)
    texture = a.node(material, ue.MaterialExpressionTextureSampleParameter2D, family + 'FieldTexture',
                     parameter_name=family + 'TurfTexture',
                     texture=ue.load_asset(f'/OpenGolfSavanna/Textures/CutTurf/{maps}/T_Turf{maps}_Color'),
                     sampler_type=ue.MaterialSamplerType.SAMPLERTYPE_COLOR)
    a.connect(uv, texture, 'UVs')
    def color(name):
        return (a.vector(material, family + name, (.07, .15, .03, 1), family), 'RGB')
    return a.custom(material, family + 'FieldColor', palette.FAMILY_COLOR_HLSL,
                    {'C': (texture, 'RGB'), 'P': (position, ''), 'Hue': (hue, ''), 'Bright': (bright, ''),
                     'T': color('TargetColor'), 'Dry': color('DryColor'), 'Lush': color('LushColor'),
                     'M': color('TextureMean'), 'HueAmount': scalar('HueAmount'),
                     'Detail': scalar('DetailStrength'), 'Strength': scalar('MacroStrength'),
                     'Contrast': scalar('StripeContrast'), 'Period': scalar('StripePeriodCm')},
                    ue.CustomMaterialOutputType.CMOT_FLOAT3)


def _field_inputs(material, position):
    origin = a.vector(material, 'FieldOriginCm', (0, 0, 0, 0), GROUP)
    forward = a.vector(material, 'FieldForward', (1, 0, 0, 0), GROUP)
    bounds = a.vector(material, 'FieldBoundsM', (0, 0, 1, 1), GROUP)
    uv = a.custom(material, 'TurfFieldUV',
                  'float2 q=(P.xy-O.xy)/100.0; float2 L=float2(dot(q,F.xy),dot(q,float2(-F.y,F.x)));'
                  'return (L-B.xy)/max(B.zw,0.001);',
                  {'P': (position, ''), 'O': (origin, 'RGBA'), 'F': (forward, 'RGBA'), 'B': (bounds, 'RGBA')},
                  ue.CustomMaterialOutputType.CMOT_FLOAT2)
    texture = field_texture()
    # The baked field is uncompressed vector data (linear colour); the placeholder is a mask.
    baked = texture.get_path_name().split('.')[0] == FIELD_PATH
    sample = a.node(material, ue.MaterialExpressionTextureSampleParameter2D, 'TurfFieldSample',
                    parameter_name='CourseField', texture=texture,
                    sampler_type=ue.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR if baked
                    else ue.MaterialSamplerType.SAMPLERTYPE_MASKS)
    a.connect(uv, sample, 'UVs')
    wobble = a.custom(material, 'TurfEdgeWobble', terrain.value_noise(70, seed=3), {'P': (position, '')},
                      ue.CustomMaterialOutputType.CMOT_FLOAT1)
    inputs = {'S': (sample, 'RGBA'), 'R': (a.vector(material, 'FieldRangesM', (2.5, 1.5, 1.5, 1.5), GROUP), 'RGBA'),
              'P': (position, ''), 'O': (origin, 'RGBA'), 'F': (forward, 'RGBA'), 'N': (wobble, ''),
              'TeeBox': (a.vector(material, 'TeeBoxM', (0, 0, 2.5, 2.5), GROUP), 'RGBA')}
    names = {'Wob': 'EdgeWobbleM', 'Mow': 'MowEdgeM', 'W': 'FirstCutWidthM', 'Outer': 'FirstCutOuterEdgeM',
             'LipW': 'LipWidthM', 'LipShade': 'LipShade'}
    inputs.update({pin: (a.scalar(material, name, EDGES[name], GROUP), '') for pin, name in names.items()})
    return inputs


def patches(material, position):
    """World-space patch weights shared by every family: hue (dry to lush) and brightness, each in 0..1."""
    def noise(name, cell, seed):
        code = terrain.value_noise(cell, seed=seed).replace(
            'return lerp(lerp(v.x,v.y,f.x),lerp(v.z,v.w,f.x),f.y);',
            f'float n=lerp(lerp(v.x,v.y,f.x),lerp(v.z,v.w,f.x),f.y);'
            f'return lerp(n,0.5,smoothstep({cell * .25},{float(cell)},max(fwidth(P.x),fwidth(P.y))));')
        return (a.custom(material, name, code, {'P': (position, '')},
                         ue.CustomMaterialOutputType.CMOT_FLOAT1), '')
    octaves = {pin: noise('TurfHueNoise' + pin, cell, 11 + i)
               for i, (pin, cell) in enumerate(zip(('A', 'B', 'Cn'), palette.HUE_CELLS_CM))}
    hue = a.custom(material, 'TurfFieldHue', palette.HUE_HLSL, octaves, ue.CustomMaterialOutputType.CMOT_FLOAT1)
    bright = a.custom(material, 'TurfFieldBright', palette.BRIGHT_HLSL,
                      {'Macro': noise('TurfFieldMacro', 1900, 0), 'D': noise('TurfBrightNoise', palette.BRIGHT_CELL_CM, 14)},
                      ue.CustomMaterialOutputType.CMOT_FLOAT1)
    return hue, bright


def field_color(material, base=None):
    """The turf colour at this pixel. base is the ground beyond the first cut; the course ground defaults to first cut."""
    position = a.node(material, ue.MaterialExpressionWorldPosition, 'TurfWorldPosition')
    hue, bright = patches(material, position)
    colors = {family: (family_color(material, family, position, hue, bright), '') for family in FAMILIES}
    inputs = dict(_field_inputs(material, position), Base=base or colors['FirstCut'], **colors)
    return a.custom(material, 'TurfFieldColor', COLOR_HLSL, inputs, ue.CustomMaterialOutputType.CMOT_FLOAT3)


def field_coverage(material):
    """How much of this pixel is maintained turf (first cut or finer) rather than the ground beyond."""
    position = a.node(material, ue.MaterialExpressionWorldPosition, 'TurfWorldPosition')
    return a.custom(material, 'TurfFieldCoverage', COVERAGE_HLSL, _field_inputs(material, position),
                    ue.CustomMaterialOutputType.CMOT_FLOAT1)


def surface_normal(material, family, parameter=None):
    """World-aligned blade relief follows the ground slope and fades before it sparkles."""
    position = a.node(material, ue.MaterialExpressionWorldPosition, 'TurfWorldPosition')
    uv = a.custom(material, family + 'ReliefUV', 'return P.xy/50.0;', {'P': (position, '')},
                  ue.CustomMaterialOutputType.CMOT_FLOAT2)
    sample = a.node(material, ue.MaterialExpressionTextureSampleParameter2D, family + 'ReliefMap',
                    parameter_name=parameter or family + 'ReliefMap',
                    texture=ue.load_asset(f'/OpenGolfSavanna/Textures/CutTurf/{family}/T_Turf{family}_NormalDX'),
                    sampler_type=ue.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    a.connect(uv, sample, 'UVs')
    return a.custom(material, family + 'Relief',
                    'float f=1-smoothstep(400.0,1800.0,D);'
                    'float3 n=normalize(G); float3 p=float3(N.xy*S*f,0);'
                    'return normalize(n+p-n*dot(n,p));',
                    {'N': (sample, 'RGB'),
                     'G': (a.node(material, ue.MaterialExpressionVertexNormalWS, 'TurfGroundNormal'), ''),
                     'D': (a.node(material, ue.MaterialExpressionPixelDepth, 'TurfReliefDepth'), ''),
                     'S': (a.scalar(material, family + 'ReliefStrength', .55, family), '')},
                    ue.CustomMaterialOutputType.CMOT_FLOAT3)


def profiles(source):
    """Per-family parameter values: palette tones, and the colour bakes' measured means so each texture keeps its grain."""
    means_file = Path(source) / 'texture-means.json'
    if not means_file.exists():
        raise RuntimeError('BakeCutTurf.py must record texture-means.json before the turf field is configured')
    means = json.loads(means_file.read_text())
    result = {}
    for family in FAMILIES:
        maps = MAPS.get(family, family)
        values = dict(DEFAULTS, **FAMILY_DEFAULTS.get(family, {}), HueAmount=palette.HUE_AMOUNT[family],
                      MacroStrength=palette.MACRO_STRENGTH[family])
        tones = palette.PALETTE[family]
        values.update({'TargetColor': a.linear_color(tones['Mid']), 'DryColor': a.linear_color(tones['Dry']),
                       'LushColor': a.linear_color(tones['Lush'])})
        values['TextureMean'] = (*means[maps], 1.0)
        values['TurfTexture'] = ue.load_asset(f'/OpenGolfSavanna/Textures/CutTurf/{maps}/T_Turf{maps}_Color')
        result[family] = values
    return result
