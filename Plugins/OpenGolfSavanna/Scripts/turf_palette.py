# SPDX-License-Identifier: MIT
"""Turf and rough-grass colours, and the colour HLSL that mixes them. Pure Python, so it can be tested without Unreal.

Real maintained turf is not one green: it drifts between drier, yellower patches and lusher, bluer
ones over a few metres. Each turf family has a mid tone and two patch tones (sRGB hex).

Calibration (2026-11-20): the owner's reference screenshots render, as sRGB means:
- dark olive rough `323C12` (73 degrees hue, 0.155 lightness, 0.53 saturation), with near-black gaps between blades;
- first cut `708036` and fairway `6D7E3C`, about equally light (0.36);
- green about `77883F` (0.39), a little lighter than the fairway.
Each family's tones moved by the hue, saturation and lightness difference between that and the range's Play captures
(AfterRough-20261121-065541, ColorMatch1b-20261121-071548, then a viewport capture of the green and View0's tee), so their dry and lush patches keep their relationships. Collar takes the mean of the
fairway and green changes, the tee the green's. Earlier owner samples (2026-10-27) set darker, more mottled tones.
On 2026-11-23 the collar took the first cut's tones and patch settings, and the first cut stopped being drawn.
The Blender colours in blender/turf_rules.py are only the bake base: the turf field rescales each
texture to these tones.
"""
import colorsys

PALETTE = {
    'Fairway': {'Mid': '627240', 'Dry': '727B40', 'Lush': '4D6336'},
    # The fairway's first cut is no longer drawn (2026-11-23): the fairway meets the rough directly. Its tones stay as
    # the green fringe's colour and as a rollback.
    'FirstCut': {'Mid': '5F6F37', 'Dry': '717837', 'Lush': '4D612E'},
    # The fringe around the green: the owner asked for the former first cut's colour (2026-11-23); the earlier collar
    # tones (Mid 63763F) rendered too light beside the green.
    'Collar': {'Mid': '5F6F37', 'Dry': '717837', 'Lush': '4D612E'},
    'Green': {'Mid': '607140', 'Dry': '69753F', 'Lush': '556A39'},
    'Tee': {'Mid': '667843', 'Dry': '707C42', 'Lush': '576E3A'},
    # The 50 mm rough: dark olive, far darker than the first cut, like the reference screenshots.
    'Rough': {'Mid': '435126', 'Dry': '535C2C', 'Lush': '374622'},
}
# How far each family moves towards its patch tones (0 = mid tone only) and its brightness variation.
# Full patch tones read as blotches in Play (calibration round 1); about half matches the samples.
HUE_AMOUNT = {'Fairway': .22, 'FirstCut': .25, 'Collar': .25, 'Green': .12, 'Tee': .15, 'Rough': .30}
MACRO_STRENGTH = {'Fairway': .20, 'FirstCut': .20, 'Collar': .20, 'Green': .035, 'Tee': .05, 'Rough': .20}
# Smaller patches carry the texture between individual blades and broad colour changes.
HUE_CELLS_CM = (700, 90, 25)
BRIGHT_CELL_CM = 65
# Rough blades darken towards a near-black thatch at the root and turn a lighter olive at the tip, like the
# reference's dark gaps between blades. BladeHeight red runs from 0 at the root to 1 at the tallest point of the
# tile. Hex values are sRGB.
ROUGH_BLADE = {'Thatch': '1F2609', 'Tip': '5A6630', 'BaseEnd': .45, 'TipStart': .6, 'TipAmount': .5}
# Tall rough (MI_GrassMedium01, Poly Haven Grass Medium). No longer placed on the range (2026-11-20); kept for the
# savanna generator and as a rollback. Its pale blades read near white under the range's
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


def _luma(rgb):
    return .2126 * rgb[0] + .7152 * rgb[1] + .0722 * rgb[2]


def rough_blade_tints():
    """Linear multipliers of the rough mid tone: (thatch, tip). The tip tint keeps the mid tone's brightness and
    changes only its hue, so blades seen from above still match the rough ground under them."""
    mid = linear(PALETTE['Rough']['Mid'])
    thatch = tuple(t / m for t, m in zip(linear(ROUGH_BLADE['Thatch']), mid))
    tip = tuple(t / m for t, m in zip(linear(ROUGH_BLADE['Tip']), mid))
    scale = _luma(mid) / _luma([t * m for t, m in zip(tip, mid)])
    return thatch, tuple(v * scale for v in tip)


# The savanna ground (M_SavannaGround's RoughLow/RoughHigh, linear) shows only where the rough fades into the
# desert, so its rough colours follow the rough palette.
ROUGH_UNDERLAY = {'RoughLow': linear(PALETTE['Rough']['Lush']), 'RoughHigh': linear(PALETTE['Rough']['Dry'])}
