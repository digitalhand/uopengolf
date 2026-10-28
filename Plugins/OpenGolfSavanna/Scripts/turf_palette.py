# SPDX-License-Identifier: MIT
"""Turf and rough-grass colours, and the colour HLSL that mixes them. Pure Python, so it can be tested without Unreal.

Real maintained turf is not one green: it drifts between drier, yellower patches and lusher, bluer
ones over a few metres. Each turf family has a mid tone and two patch tones (sRGB hex). The owner's
fairway sample measures about 74 degrees hue and 0.37 saturation; these are the starting values,
calibrated against Play captures. A later sample (2026-10-27) matched that hue but rendered about 10%
darker and more mottled, so fairway and first cut were darkened 10% and patch more strongly.
The Blender colours in blender/turf_rules.py are only the bake base: the turf field rescales each
texture to these tones.
"""
import colorsys

PALETTE = {
    'Fairway': {'Mid': '576739', 'Dry': '667039', 'Lush': '455A30'},
    'FirstCut': {'Mid': '4D5C33', 'Dry': '5A6433', 'Lush': '3E512B'},
    'Collar': {'Mid': '627742', 'Dry': '717F41', 'Lush': '526B38'},
    'Green': {'Mid': '688046', 'Dry': '728544', 'Lush': '5C783E'},
    'Tee': {'Mid': '6B8046', 'Dry': '768544', 'Lush': '5B763D'},
}
# How far each family moves towards its patch tones (0 = mid tone only) and its brightness variation.
# Full patch tones read as blotches in Play (calibration round 1); about half matches the samples.
HUE_AMOUNT = {'Fairway': .22, 'FirstCut': .25, 'Collar': .2, 'Green': .12, 'Tee': .15}
MACRO_STRENGTH = {'Fairway': .20, 'FirstCut': .20, 'Collar': .07, 'Green': .035, 'Tee': .05}
# Smaller patches carry the texture between individual blades and broad colour changes.
HUE_CELLS_CM = (700, 90, 25)
BRIGHT_CELL_CM = 65
# Tall rough (MI_GrassMedium01, Poly Haven Grass Medium). Its pale blades read near white under the range's
# sun, so they are darkened; the earlier saturation boost (1.6) and pure-green tint read as a dark,
# blue-free green. An olive tint and a little dryness give the samples' rough; more dryness reads as dead grass. Hex values are sRGB.
GRASS_CELLS_CM = (300, 900)
ROUGH_GRASS = {'Brightness': .7, 'Saturation': 1.0, 'Tint': 'B2CB90', 'DryNearPlay': .1, 'DryJitter': .35}

# Fine patch weights dominate; the broad field stays subtle.
HUE_HLSL = 'return 0.2*A+0.4*B+0.4*Cn;'
BRIGHT_HLSL = 'return 0.2*Macro+0.8*D;'
# The mid tone moves towards Dry or Lush, the texture keeps its own grain around that tone, and stripes
# fade out as their screen footprint grows, so far stripes cannot alias.
FAMILY_COLOR_HLSL = ('float h=(smoothstep(0.15,0.85,Hue)-0.5)*2*HueAmount;'
                     'float3 base=h<0?lerp(T,Lush,-h):lerp(T,Dry,h);'
                     'float3 tex=C*base/max(M,0.001);'
                     'float stripe=0.5+0.5*sin(P.y/max(Period,1.0)*3.14159265);'
                     'float band=Contrast*(stripe-0.5)*(1-smoothstep(40.0,200.0,fwidth(P.y)*400.0/max(Period,1.0)));'
                     'return lerp(base,tex,Detail)*(1+(Bright-0.5)*Strength+band);')
# Two octaves stretched back towards the full 0..1 range.
GRASS_PATCH_HLSL = 'return saturate(0.5+(0.6*A+0.4*B-0.5)*1.8);'
# Dryness varies with world position, never per instance: a tiled square of tufts is one instance.
GRASS_COLOR_HLSL = ('float dryness=saturate(lerp(Far,Near,M)+(Noise-0.5)*Jit);'
                    'float3 c=lerp(A,D*DT,dryness)*T*lerp(1,O,S)*B;'
                    'return lerp(dot(c,float3(0.2126,0.7152,0.0722)).xxx,c,Sat);')


def _rgb(hex_rgb):
    return tuple(int(hex_rgb[i:i + 2], 16) / 255 for i in (0, 2, 4))


def hls(hex_rgb):
    """Hue in degrees, lightness and saturation of an sRGB hex colour."""
    hue, lightness, saturation = colorsys.rgb_to_hls(*_rgb(hex_rgb))
    return hue * 360, lightness, saturation


def linear(hex_rgb):
    """Linear RGB of an sRGB hex colour, as material vector parameters store it."""
    return tuple(v / 12.92 if v <= 0.04045 else ((v + 0.055) / 1.055) ** 2.4 for v in _rgb(hex_rgb))
