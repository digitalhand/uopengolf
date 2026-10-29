# SPDX-License-Identifier: MIT
"""How vegetation moves in the wind: response profiles, the World Position Offset HLSL, and a Python
mirror of the same maths for tests. Pure Python, so it can be tested without Unreal.

Wind pushes a plant over; it does not swing it to and fro. Each layer of a plant is a damped spring
loaded by drag:
  bend     the whole plant leans downwind about its base, growing with height as (h / H) ** exponent;
           points drop as they lean, keeping their distance from the base
  limbs    branches stream downwind, more toward the crown's edge and top
  leaves   leaves and blade tips stream a little and flutter, the only symmetric motion
Drag grows with the square of the wind, so a layer's load is C = (u / U_half) ** m, and it bends
C / (1 + C) of its largest deflection: always downwind, saturating as a plant streamlines in a gale
(de Langre 2008, Annu. Rev. Fluid Mech. 40). U_half, the 10 m wind that bends a layer half way, is its
stiffness against its drag area: what makes a blade of grass go over before a branch, and a branch
before a trunk. Deflections are for the plant at its profile's size and grow in proportion with the mesh
and the instance's scale, so a plant placed larger leans the same angle and moves farther.

Gusts are frozen turbulence carried downwind at the wind's speed (Taylor): four waves, 80 to 5 m long,
whose strength follows the inertial range (amplitude ~ wavelength ** 1/3). Their size relative to the
wind is the turbulence intensity over the course's roughness, 1 / ln(z / z0) (EN 1991-1-4, 4.4), taken
at the plant's height and no lower than 2 m. A gust passes a point at speed / wavelength Hz, and each
layer answers it as a damped oscillator with its own natural frequency and damping. That is where
weight shows: a heavy trunk follows only the long gusts and sways near its own frequency, light grass
follows nearly all of them, and gusts roll across grass as waves (Py, de Langre and Moulia 2006,
J. Fluid Mech. 568). Gusts add to the load, so a surge leans a plant further and a lull lets it part
way back, never upwind of upright.

Every input is a smooth function of the rest position and instance transform, never a hash of one: the
base of a tree is rebuilt at each vertex in float32 and differs by hundredths of a millimetre, which a
hash turns into a different phase per vertex. Trees take their gust at their base, so a tree moves as
one; limbs and grass take it where they are. Trunk, branch and leaf materials of a tree compute the same
bend and sway, so they stay joined, with leaves adding only their own stream and flutter; neighbouring
grass tiles meet without seams. Calm air is exactly still.

A profile is data. Unreal-side wiring is in vegetation_wind_assets.py; what uses which profile is in
the savanna kit's vegetation_wind_assignments.py. The Beaufort scale (OpenGolfBeaufort.h, read below)
is the calibration the tests hold the profiles to.
"""
from dataclasses import dataclass
from pathlib import Path
import math
import re

BEAUFORT_HEADER = Path(__file__).resolve().parents[1] / "Source/OpenGolfCourse/Public/OpenGolfBeaufort.h"
_ROW = re.compile(r"\{\s*(\d+),\s*([0-9.]+)f,\s*TEXT\(\"([^\"]+)\"\)")


def _beaufort():
    """Lowest 10 m wind of each force, m/s, from the C++ table (the single source)."""
    rows = [(int(number), float(speed), name) for number, speed, name in _ROW.findall(BEAUFORT_HEADER.read_text())]
    if [number for number, _, _ in rows] != list(range(13)):
        raise RuntimeError(f"Could not read the Beaufort scale from {BEAUFORT_HEADER}")
    return tuple(speed for _, speed, _ in rows), tuple(name for _, _, name in rows)


BEAUFORT_MIN_MPS, BEAUFORT_NAMES = _beaufort()


def force_mps(force):
    return BEAUFORT_MIN_MPS[force]


# Gust waves: (wavelength m, amplitude, phase, phase of its crosswind part). Amplitudes ~ wavelength ** 1/3,
# with a unit sum of squares.
GUSTS = ((80.0, 0.714, 0.0, 2.1), (30.0, 0.515, 1.7, 4.4), (12.0, 0.38, 4.1, 0.9), (5.0, 0.283, 2.6, 3.3))
# Fastest 10 m wind the offset bound is sized for (45 mph, above the range's 40 mph control). Faster
# winds are clamped to the bound by the material.
DESIGN_MPS = 20.1


@dataclass(frozen=True)
class Layer:
    """A part of a plant that bends as one: a spring loaded by drag."""
    # Largest deflection in cm, approached in a gale; or, for grass, the largest lean in degrees.
    max_cm: float = 0.0
    max_deg: float = 0.0
    # 10 m wind (m/s) that bends it half way: its stiffness against its drag area.
    half_mps: float = 1.0
    # Its free sway frequency (Hz): lighter or stiffer parts are faster.
    natural_hz: float = 1.0
    # Share of critical damping, from its wood and from the air on its leaves.
    damping: float = 1.0

    def max_at(self, height_cm, size=1.0):
        """Largest deflection of a mesh height_cm tall at scale 1; size is that height over the profile's."""
        return self.max_cm * size if self.max_cm else height_cm * math.sin(math.radians(self.max_deg))


@dataclass(frozen=True)
class Leaves:
    """Leaves or blade tips: stream downwind and flutter at their own frequency."""
    flutter_cm: float = 0.0
    stream_cm: float = 0.0
    half_mps: float = 1.0
    hz: float = 1.0


STILL = Layer()


@dataclass(frozen=True)
class WindResponse:
    name: str
    bend: Layer
    limbs: Layer
    leaves: Leaves
    # Bend grows with height as (h / H) ** exponent: 2 is a stiff cantilever (trunks), lower is softer.
    bend_exponent: float
    # Leaf motion grows with height as (h / H) ** this: 0 everywhere (tree leaves), 2 at blade tips (grass).
    leaf_exponent: float
    # True: gusts vary across the plant (grass tiles, which span a metre). False: one gust per plant, at
    # its base, so a tree's parts move together.
    vertex_gusts: bool
    # Drag ~ wind ** this; 2 for a rigid body. Saturation in C / (1 + C) adds the streamlining.
    drag_exponent: float = 2.0
    # Height (cm) of the plant the centimetre values describe; 0 is the mesh's own height at scale 1.
    size_cm: float = 0.0


# Grass Medium rough (about 36 cm). Light, flexible blades lie over in strong wind and follow nearly every
# gust; crops and grasses wave near 1-2 Hz with heavy aerodynamic damping (Py et al. 2006).
TALL_GRASS = WindResponse(
    "TallGrass",
    bend=Layer(max_deg=60.0, half_mps=7.0, natural_hz=1.5, damping=0.25),
    limbs=STILL,
    leaves=Leaves(flutter_cm=0.6, half_mps=5.0, hz=3.0),
    bend_exponent=1.5, leaf_exponent=2.0, vertex_gusts=True)

# Nature Essentials tufts under the trees (about 70 cm). Seed heads make them heavier and slower.
UNDERSTORY_GRASS = WindResponse(
    "UnderstoryGrass",
    bend=Layer(max_deg=55.0, half_mps=10.0, natural_hz=1.0, damping=0.25),
    limbs=STILL,
    leaves=Leaves(flutter_cm=0.5, half_mps=5.0, hz=2.0),
    bend_exponent=1.5, leaf_exponent=2.0, vertex_gusts=True)

# Broadleaf trees such as the jacaranda, described at 9 m: the range places its 19.6 m jacaranda mesh at
# about 0.46 scale (savanna_layout). Trees sway at 0.2-0.6 Hz, with damping from 0.05 bare to 0.1-0.3 in
# leaf (Moore and Maguire 2004, Trees 18). Leaves rustle at force 2, small branches move at 4 and large
# ones at 6, and whole trees move at 7.
BROADLEAF_TREE = WindResponse(
    "BroadleafTree",
    bend=Layer(max_cm=150.0, half_mps=25.0, natural_hz=0.35, damping=0.15),
    limbs=Layer(max_cm=40.0, half_mps=12.0, natural_hz=1.2, damping=0.15),
    leaves=Leaves(flutter_cm=0.8, stream_cm=3.0, half_mps=4.0, hz=2.5),
    bend_exponent=2.0, leaf_exponent=0.0, vertex_gusts=False, size_cm=900.0)

PROFILES = {profile.name: profile for profile in (TALL_GRASS, UNDERSTORY_GRASS, BROADLEAF_TREE)}


def parameters(profile, height_cm, crown_radius_cm):
    """Material instance vector parameters for a profile on a mesh of the given size at scale 1. Their
    centimetres are at scale 1; the material multiplies them by the instance's scale."""
    bend, limbs, leaves = profile.bend, profile.limbs, profile.leaves
    size = height_cm / profile.size_cm if profile.size_cm else 1.0
    return {
        "WindBend": (bend.max_at(height_cm, size), bend.half_mps, bend.natural_hz, bend.damping),
        "WindSway": (limbs.max_at(height_cm, size), limbs.half_mps, limbs.natural_hz, limbs.damping),
        "WindFlutter": (leaves.flutter_cm * size, leaves.half_mps, leaves.hz, leaves.stream_cm * size),
        "WindShape": (float(height_cm), float(crown_radius_cm), profile.bend_exponent, profile.leaf_exponent),
        "WindDetail": (profile.drag_exponent, 1.0 if profile.vertex_gusts else 0.0, 0.0, 0.0),
    }


def max_displacement_cm(params, max_scale=1.0):
    """Bound on the offset along any axis up to DESIGN_MPS, for instances placed up to max_scale: the
    material's Nanite and culling bounds."""
    def bent(vector):
        return vector[0] * _share(params["WindDetail"][0], DESIGN_MPS, vector[1], 1.0, 0.6)
    flutter = params["WindFlutter"]
    return math.ceil(1.1 * max_scale * (1.12 * (bent(params["WindBend"]) + bent(params["WindSway"])) + flutter[0] + flutter[3]))


# Inputs: A the vertex's offset from the instance origin in world space (cm, scaled), Up the instance's
# up axis in world space (scaled), P the rest world position (cm), T time (s), Wind and Air
# from MPC_Wind (see UOpenGolfWindSubsystem), Bend/Sway/Flutter/Shape/Detail from parameters(). Returns
# the world offset in cm. Keep in step with offset() below; {count} and {gusts} are filled from GUSTS.
_HLSL = """float3 O = 0;
float speed = Wind.z;
if (speed <= 0.001) return O;
float2 dir = Wind.xy / speed;
float3 D = float3(dir, 0);
float3 S = float3(-dir.y, dir.x, 0);
float scale = max(length(Up), 1e-4);
float3 up = Up / scale;
float h = max(dot(A, up), 0);
float eta = saturate(h / max(Shape.x * scale, 1));
float rho = saturate(length(A - up * dot(A, up)) / max(Shape.y * scale, 1));
float ti = 1 / log(max(Shape.x * scale * 0.01, 2) / max(Air.x, 0.001));
float2 GB = (Detail.y > 0.5 ? P.xy : P.xy - A.xy) * 0.01;
float2 GS = P.xy * 0.01;
float drift = T * speed + Air.y;
float4 gusts[{count}] = {{{gusts}}};
float2 bendGust = 0;
float2 limbGust = 0;
for (int i = 0; i < {count}; i++)
{{
    float4 g = gusts[i];
    float2 r = speed / g.x / max(float2(Bend.z, Sway.z), 0.01);
    float2 re = 1 - r * r;
    float2 im = 2 * float2(Bend.w, Sway.w) * r;
    float2 gain = min(rsqrt(max(re * re + im * im, 1e-4)), 2.5);
    float2 lag = atan2(im, re);
    float2 along = float2(dot(GB, dir), dot(GS, dir)) - drift;
    float2 side = float2(dot(GB, S.xy), dot(GS, S.xy));
    float2 wave = 6.2832 * along / g.x + 1.5708 * sin(3.696 * side / g.x + g.z) + g.z - lag;
    bendGust += g.y * gain.x * float2(sin(wave.x), sin(wave.x + g.w));
    limbGust += g.y * gain.y * float2(sin(wave.y), sin(wave.y + g.w));
}}
float m = Detail.x;
float bendShare = 1 / (1 + exp(-m * (log(speed / max(Bend.y, 0.01)) + clamp(ti * bendGust.x, -0.6, 0.6))));
float bend = Bend.x * scale * bendShare * exp2(Shape.z * log2(max(eta, 1e-4)));
float bendSide = bend * clamp(ti * bendGust.y, -0.5, 0.5);
O += D * bend + S * bendSide;
float lateral = min(length(float2(bend, bendSide)), 0.9 * h);
O -= up * (h - sqrt(max(h * h - lateral * lateral, 0)));
if (Sway.x > 0)
{{
    float limbShare = 1 / (1 + exp(-m * (log(speed / max(Sway.y, 0.01)) + clamp(ti * limbGust.x, -0.6, 0.6))));
    float limb = Sway.x * scale * limbShare * rho * eta;
    O += (D - up * 0.15) * limb + S * (limb * clamp(ti * limbGust.y, -0.5, 0.5));
}}
float leafShare = 1 / (1 + exp(-m * (log(speed / max(Flutter.y, 0.01)) + clamp(ti * bendGust.x, -0.6, 0.6))));
float reach = leafShare * exp2(Shape.w * log2(max(eta, 1e-4)));
float3 Pm = P * 0.01;
float leaf = 3 * (sin(Pm.x * 23 + Pm.z * 17) + sin(Pm.y * 19 - Pm.x * 11));
O += D * (Flutter.w * scale * reach) + (up * 0.8 + S * 0.4 + D * 0.3) * (Flutter.x * scale * reach * sin(6.2832 * Flutter.z * T + leaf));
return O;"""

WIND_HLSL = _HLSL.format(count=len(GUSTS), gusts=", ".join(f"float4({', '.join(f'{v:g}' for v in gust)})" for gust in GUSTS))


def _dot(a, b):
    return sum(p * q for p, q in zip(a, b))


def _add(a, b, scale=1.0):
    return tuple(p + q * scale for p, q in zip(a, b))


def _clamp(x, low, high):
    return min(max(x, low), high)


def _share(m, speed, half, ti, gust):
    """Share of a layer's largest deflection: C / (1 + C), C = (u / half) ** m with the gust in u."""
    return 1 / (1 + math.exp(-m * (math.log(speed / max(half, 0.01)) + _clamp(ti * gust, -0.6, 0.6))))


def _gusts(bend_v, sway_v, gb, gs, direction, side_axis, drift, speed):
    """The bend's gust at gb and the limbs' at gs, each through its layer's oscillator: (along, across) each."""
    bend_gust, limb_gust = [0.0, 0.0], [0.0, 0.0]
    for wavelength, amplitude, phase, side_phase in GUSTS:
        for gust, point, hz, damping in ((bend_gust, gb, bend_v[2], bend_v[3]), (limb_gust, gs, sway_v[2], sway_v[3])):
            r = speed / wavelength / max(hz, 0.01)
            re, im = 1 - r * r, 2 * damping * r
            gain = min(1 / math.sqrt(max(re * re + im * im, 1e-4)), 2.5)
            lag = math.atan2(im, re)
            along = _dot(point, direction) - drift
            side = _dot(point, side_axis)
            wave = 6.2832 * along / wavelength + 1.5708 * math.sin(3.696 * side / wavelength + phase) + phase - lag
            gust[0] += amplitude * gain * math.sin(wave)
            gust[1] += amplitude * gain * math.sin(wave + side_phase)
    return bend_gust, limb_gust


def offset(params, arm, up, position, time, wind, air=(0.03, 0.0, 1.225, 0.0)):
    """Python mirror of WIND_HLSL: the world offset in cm. params is parameters(...); wind is MPC Wind."""
    bend_v, sway_v, flutter_v = params["WindBend"], params["WindSway"], params["WindFlutter"]
    shape, detail = params["WindShape"], params["WindDetail"]
    out = (0.0, 0.0, 0.0)
    speed = wind[2]
    if speed <= 0.001:
        return out
    direction = (wind[0] / speed, wind[1] / speed)
    d3, s3 = (direction[0], direction[1], 0.0), (-direction[1], direction[0], 0.0)
    scale = max(math.sqrt(_dot(up, up)), 1e-4)
    up_n = tuple(v / scale for v in up)
    h = max(_dot(arm, up_n), 0.0)
    eta = _clamp(h / max(shape[0] * scale, 1.0), 0.0, 1.0)
    radial = _add(arm, up_n, -_dot(arm, up_n))
    rho = _clamp(math.sqrt(_dot(radial, radial)) / max(shape[1] * scale, 1.0), 0.0, 1.0)
    ti = 1 / math.log(max(shape[0] * scale * 0.01, 2.0) / max(air[0], 0.001))
    gb = tuple(v * 0.01 for v in (position[:2] if detail[1] > 0.5 else _add(position[:2], arm[:2], -1.0)))
    gs = tuple(v * 0.01 for v in position[:2])
    drift = time * speed + air[1]
    bend_gust, limb_gust = _gusts(bend_v, sway_v, gb, gs, direction, s3[:2], drift, speed)
    m = detail[0]
    bend = bend_v[0] * scale * _share(m, speed, bend_v[1], ti, bend_gust[0]) * 2 ** (shape[2] * math.log2(max(eta, 1e-4)))
    bend_side = bend * _clamp(ti * bend_gust[1], -0.5, 0.5)
    out = _add(_add(out, d3, bend), s3, bend_side)
    lateral = min(math.hypot(bend, bend_side), 0.9 * h)
    out = _add(out, up_n, -(h - math.sqrt(max(h * h - lateral * lateral, 0.0))))
    if sway_v[0] > 0:
        limb = sway_v[0] * scale * _share(m, speed, sway_v[1], ti, limb_gust[0]) * rho * eta
        out = _add(_add(out, _add(d3, up_n, -0.15), limb), s3, limb * _clamp(ti * limb_gust[1], -0.5, 0.5))
    reach = _share(m, speed, flutter_v[1], ti, bend_gust[0]) * 2 ** (shape[3] * math.log2(max(eta, 1e-4)))
    pm = tuple(v * 0.01 for v in position)
    leaf = 3 * (math.sin(pm[0] * 23 + pm[2] * 17) + math.sin(pm[1] * 19 - pm[0] * 11))
    flutter_axis = _add(_add(tuple(v * 0.8 for v in up_n), s3, 0.4), d3, 0.3)
    out = _add(out, d3, flutter_v[3] * scale * reach)
    return _add(out, flutter_axis, flutter_v[0] * scale * reach * math.sin(6.2832 * flutter_v[2] * time + leaf))
