# SPDX-License-Identifier: MIT
"""One world-space turf field shared by the course ground and the cut-turf blades.

Every ground material and the blades evaluate the same function of world position, so a seam
between two ground materials shows no change of colour and a blade matches the ground under it.
Layers, lowest first: rough, fairway, bunker lip, collar, green, tee. The rough layer is the 50 mm cut-turf rough
(2026-11-20); the savanna ground blends it out into the desert beyond the rough band. The fairway meets the rough at a
mowing line: its first-cut band was removed on 2026-11-23, leaving a fringe (collar) only around the green. Signed
distances come from T_CourseSurfaceField (turf_mask.py); the square tee box is analytic. Each family mixes its mid tone
with drier and lusher patch tones (turf_palette.py) by world-space noise.
"""
import json
from pathlib import Path

import unreal as ue
import course_assets as a
import BuildTerrainAssets as terrain
import turf_palette as palette

FAMILIES = ('Fairway', 'Collar', 'Green', 'Tee', 'Rough')
# The fairway's first cut, no longer drawn (2026-11-23); drop_retired_nodes removes its field nodes.
RETIRED = ('FirstCut',)
MAPS = {'Tee': 'Collar'}  # The tee is finely cut, so it shares the collar's maps.
SCALARS = ('TextureSizeCm', 'DetailStrength', 'MacroStrength', 'StripeContrast', 'StripePeriodCm', 'HueAmount')
VECTORS = ('TargetColor', 'DryColor', 'LushColor', 'TextureMean')
DEFAULTS = {'TextureSizeCm': 50, 'DetailStrength': 1.35, 'MacroStrength': .15, 'StripeContrast': 0, 'StripePeriodCm': 400,
            'HueAmount': 1}
# The rough stretches its baked blade-to-gap contrast around the same mean, for the reference's dark gaps between
# blades (2026-11-20). Lower it first if the rough shimmers.
FAMILY_DEFAULTS = {'Fairway': {'StripeContrast': .06}, 'Tee': {'StripeContrast': .04, 'StripePeriodCm': 100},
                   'Rough': {'DetailStrength': 2.0}}
# M_CutTurfRough used all 16 texture samplers (savanna ground, course field, first-cut relief) before the first-cut
# layer was removed; these families' textures use the shared wrap sampler instead of their own.
SHARED_SAMPLER = ('Rough',)
EDGES = {'MowEdgeM': .06, 'EdgeWobbleM': .03, 'LipWidthM': .25, 'LipShade': .8}
RETIRED_EDGES = ('FirstCutWidthM', 'FirstCutOuterEdgeM')
FIELD_PATH = '/OpenGolfSavanna/CutTurf/Textures/T_CourseSurfaceField'
PLACEHOLDER = '/OpenGolfSavanna/Shared/Textures/T_WhiteMask'
GROUP = 'Turf field'

# Signed distances in terrain metres (negative inside), then each layer's coverage. The tee box,
# fairway, collar and green edges are mowing lines.
FIELD_HLSL = '''float4 d=(S-0.5)*2*R;
float2 q=(P.xy-O.xy)/100.0; float2 L=float2(dot(q,F.xy),dot(q,float2(-F.y,F.x)));
float n=(N-0.5)*Wob; d.xyz+=n;
float ef=max(Mow,fwidth(d.x)), eg=max(Mow,fwidth(d.y)), ec=max(Mow,fwidth(d.z));
float fairway=1-smoothstep(-ef,ef,d.x);
float collar=1-smoothstep(-ec,ec,d.z);
float green=1-smoothstep(-eg,eg,d.y);
float lip=(1-smoothstep(0,max(LipW,0.001),d.w))*smoothstep(-0.02,0.02,d.w);
float2 t=abs(L-TeeBox.xy)-TeeBox.zw; float tb=length(max(t,0))+min(max(t.x,t.y),0);
float et=max(Mow,fwidth(tb)); float tee=(TeeBox.z>0&&TeeBox.w>0)?1-smoothstep(-et,et,tb):0;
'''
COLOR_HLSL = FIELD_HLSL + '''float3 c=lerp(Base,Fairway,fairway);
c*=lerp(1,LipShade,lip);
c=lerp(c,Collar,collar);
c=lerp(c,Green,green);
return lerp(c,Tee,tee);'''
# Maintained turf on the rough ground: the fairway's mowing edge where it overlaps the rough mesh, and the tee.
COVERAGE_HLSL = FIELD_HLSL + 'return saturate(max(fairway,tee));'


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
    shared = {'sampler_source': ue.SamplerSourceMode.SSM_WRAP_WORLD_GROUP_SETTINGS} if family in SHARED_SAMPLER else {}
    texture = a.node(material, ue.MaterialExpressionTextureSampleParameter2D, family + 'FieldTexture',
                     parameter_name=family + 'TurfTexture',
                     texture=ue.load_asset(f'/OpenGolfSavanna/CutTurf/Textures/{maps}/T_Turf{maps}_D'),
                     sampler_type=ue.MaterialSamplerType.SAMPLERTYPE_COLOR, **shared)
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
    names = {'Wob': 'EdgeWobbleM', 'Mow': 'MowEdgeM', 'LipW': 'LipWidthM', 'LipShade': 'LipShade'}
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


def field_color(material, ground=None, weight=None):
    """The turf colour at this pixel; the rough lies beyond the fairway. ground and weight (0..1) are another
    ground beyond the rough band: the savanna ground keeps its desert where its rough weight falls to zero.
    A graph built before 2026-11-23 loses its first-cut layer's nodes here."""
    drop_retired_nodes(material)
    position = a.node(material, ue.MaterialExpressionWorldPosition, 'TurfWorldPosition')
    hue, bright = patches(material, position)
    colors = {family: (family_color(material, family, position, hue, bright), '') for family in FAMILIES}
    base = colors.pop('Rough')
    if ground:
        if not weight:
            raise ValueError('A ground beyond the rough needs its rough weight')
        base = (a.custom(material, 'TurfRoughBand', 'return lerp(G,R,saturate(W));',
                         {'G': ground, 'R': base, 'W': weight}, ue.CustomMaterialOutputType.CMOT_FLOAT3), '')
    inputs = dict(_field_inputs(material, position), Base=base, **colors)
    return a.custom(material, 'TurfFieldColor', COLOR_HLSL, inputs, ue.CustomMaterialOutputType.CMOT_FLOAT3)


def field_coverage(material):
    """How much of this pixel is maintained turf (the fairway's edge or the tee) rather than the rough beyond."""
    position = a.node(material, ue.MaterialExpressionWorldPosition, 'TurfWorldPosition')
    return a.custom(material, 'TurfFieldCoverage', COVERAGE_HLSL, _field_inputs(material, position),
                    ue.CustomMaterialOutputType.CMOT_FLOAT1)


def retired_node_names():
    """Descriptions of the nodes the retired layers left in a ground material's graph. A material's own first-cut relief
    (FirstCutRelief*, FirstCutRoughnessMap) is still in use and is not listed."""
    names = set()
    for family in RETIRED:
        names.update(family + suffix for suffix in ('FieldUV', 'FieldTexture', 'FieldColor'))
        names.update(f'Param.{family}{name}' for name in SCALARS + VECTORS)
    return names | {'Param.' + name for name in RETIRED_EDGES}


def drop_retired_nodes(material):
    """Delete the retired layers' unconnected nodes, so their parameters no longer show on the instances."""
    names = retired_node_names()
    retired = [node for node in a.MAT.get_material_expressions(material) if node.get_editor_property('desc') in names]
    for node in retired:
        a.MAT.delete_material_expression(material, node)
    return len(retired)


def surface_normal(material, family, parameter=None):
    """World-aligned blade relief follows the ground slope and fades before it sparkles."""
    position = a.node(material, ue.MaterialExpressionWorldPosition, 'TurfWorldPosition')
    uv = a.custom(material, family + 'ReliefUV', 'return P.xy/50.0;', {'P': (position, '')},
                  ue.CustomMaterialOutputType.CMOT_FLOAT2)
    sample = a.node(material, ue.MaterialExpressionTextureSampleParameter2D, family + 'ReliefMap',
                    parameter_name=parameter or family + 'ReliefMap',
                    texture=ue.load_asset(f'/OpenGolfSavanna/CutTurf/Textures/{family}/T_Turf{family}_N'),
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
        values['TurfTexture'] = ue.load_asset(f'/OpenGolfSavanna/CutTurf/Textures/{maps}/T_Turf{maps}_D')
        result[family] = values
    return result
