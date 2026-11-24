# SPDX-License-Identifier: MIT
"""Original cut grass, with loose clumps and curved blades. Lengths are in metres."""
from dataclasses import dataclass
import math
import random


@dataclass(frozen=True)
class Turf:
    height: float
    width: float
    clump_spacing: float
    color: str
    clump_blades: tuple[int, int] = (4, 7)
    filler_fraction: float = 0.4
    segments: int = 2
    # Blades of three or more segments lean over towards the tip by up to this many radians (see _curved_rows).
    droop: float = 0.0
    height_range: tuple[float, float] = (.74, 1.25)
    lean_range: tuple[float, float] = (.35, .95)


# Short cuts are drawn only near the camera over matching ground, so fewer, wider, single-quad blades
# resolve better than dense sub-pixel ones. The green family is baked into a texture only.
SURFACES = {
    'Fairway': Turf(.015, .0040, .0132, '496B32', (2, 4), 1.5, 1),
    'FirstCut': Turf(.030, .0038, .0166, '405E2D', (3, 5), 1.2),
    'Collar': Turf(.020, .0044, .0134, '52733A', (2, 4), 1.5, 1),
    'Green': Turf(.004, .0010, .0045, '638345', (3, 5), .25),
    # 50 mm rough after a hair-particle lawn: thin blades of even height, nearly upright at the root, leaning
    # over in their top third. Tiles are larger because the rough band is much wider than the cuts.
    'Rough': Turf(.050, .0034, .0150, '4F6525', (2, 3), 1.0, 3, droop=1.0, height_range=(.9, 1.1),
                  lean_range=(.08, .22)),
}

_SQUARES = (((.5, .5), '50cm'), ((.25, .25), '25cm'), ((.125, .125), '125mm'), ((.0625, .0625), '625mm'))
# Exported tiles per family: (size in metres, label). Tiled placement halves a tile at each level, so the
# squares must halve. Green is baked into a texture only.
FOOTPRINTS = {
    'Fairway': _SQUARES + (((.5, .05), 'Edge'),),
    'FirstCut': _SQUARES + (((.5, .05), 'Edge'),),
    'Collar': _SQUARES + (((.5, .05), 'Edge'),),
    'Rough': (((1, 1), '1m'), ((.5, .5), '50cm'), ((.25, .25), '25cm'), ((.125, .125), '125mm')),
}


def _curved_rows(rng, spec, height, lean, direction, twist):
    """A blade that stands almost upright at the root and leans over towards the tip, like a hair strand with
    endpoint roughness. Returns rows of (along, z, span, angle), where along scales the horizontal reach also
    returned, so the caller places the rows exactly as it places straight blades."""
    segments = spec.segments
    curl = spec.droop * rng.uniform(.75, 1.25)
    tip_span = rng.uniform(.12, .3)
    start = math.atan2(lean, height)
    # Rows crowd towards the tip, where the blade bends.
    stations = [(i / segments) ** .8 for i in range(segments + 1)]
    points = [(0.0, 0.0)]
    for low, high in zip(stations, stations[1:]):
        angle = start + curl * ((low + high) / 2) ** 3
        reach, z = points[-1]
        points.append((reach + (high - low) * math.sin(angle), z + (high - low) * math.cos(angle)))
    scale = height / max(z for _, z in points)
    reach = points[-1][0] * scale
    spans = [.8] + [1 - .3 * (i - 1) / max(segments - 2, 1) for i in range(1, segments)] + [tip_span]
    rows = tuple((along * scale / reach if reach > 0 else 0, z * scale, span, direction + twist * station)
                 for (along, z), span, station in zip(points, spans, stations))
    return rows, reach


@dataclass(frozen=True)
class Blade:
    vertices: tuple
    tone: float
    patch: float


def blade_layout(surface, size, seed):
    """Place loose clumps plus scattered filler; tips can overlap neighbouring tiles."""
    spec = SURFACES[surface]
    if any(not math.isfinite(v) or v <= 0 for v in size):
        raise ValueError('A grass tile needs positive, finite dimensions')
    rng = random.Random(seed)
    phase = rng.uniform(0, math.tau)
    clumps = max(1, round(size[0]*size[1]/spec.clump_spacing**2))
    roots = []
    for _ in range(clumps):
        cx, cy = (rng.uniform(-s/2, s/2) for s in size)
        direction = rng.uniform(0, math.tau)
        patch = .5 + .25*math.sin(cx*29+phase)*math.cos(cy*19-phase)
        count = rng.randint(*spec.clump_blades)
        for _ in range(count):
            angle = rng.uniform(0, math.tau)
            radius = spec.clump_spacing * .42 * math.sqrt(rng.random())
            # Wrap clumps through the tile edge instead of trimming their tips into a straight wall.
            x = (cx + math.cos(angle)*radius + size[0]/2) % size[0] - size[0]/2
            y = (cy + math.sin(angle)*radius + size[1]/2) % size[1] - size[1]/2
            roots.append((x,y,direction+rng.uniform(-2.5,2.5),patch))
    for _ in range(round(len(roots)*spec.filler_fraction)):
        x, y = (rng.uniform(-s/2, s/2) for s in size)
        roots.append((x,y,rng.uniform(0,math.tau),rng.uniform(.3,.7)))
    for x,y,direction,patch in roots:
        # Random draws stay in this order: the maintained families' meshes are fingerprinted in the tests.
        height = spec.height * max(.7,min(1.3,rng.uniform(*spec.height_range)*(.9+.2*patch)))
        width = spec.width*rng.uniform(.65,1.4)
        lean = height*rng.uniform(*spec.lean_range)
        twist = rng.uniform(-.55,.55)
        vertices = []
        if spec.segments == 1:
            # One tapered quad: widest at the root.
            rows = ((0,0,1.0,direction),(1,height,rng.uniform(.25,.5),direction+twist))
        elif spec.segments == 2:
            middle = rng.uniform(.25,.5)
            rows = ((0,0,.65,direction),(middle,height*.53,1.0,direction+twist*.4),
                    (1,height,rng.uniform(.45,.85),direction+twist))
        else:
            rows, lean = _curved_rows(rng, spec, height, lean, direction, twist)
        lx,ly = math.cos(direction)*lean,math.sin(direction)*lean
        for along,z,span,angle in rows:
            dx,dy = -math.sin(angle)*width*span/2,math.cos(angle)*width*span/2
            vertices.extend(((x+lx*along-dx,y+ly*along-dy,z),
                             (x+lx*along+dx,y+ly*along+dy,z)))
        yield Blade(tuple(vertices),rng.uniform(.12,.95),patch)


def blades(surface,size,seed):
    """Blade vertices for callers that do not need colour variation."""
    for blade in blade_layout(surface,size,seed):
        yield blade.vertices


def mesh_data(surface,size,seed):
    """Return geometry, UVs and vertex colours; red stores height, green stores blade tone."""
    vertices,faces,uvs,colors = [],[],[],[]
    for blade in blade_layout(surface,size,seed):
        start = len(vertices)
        vertices.extend(blade.vertices)
        tip = max(v[2] for v in blade.vertices)
        for row in range(len(blade.vertices)//2-1):
            a = start+2*row
            faces.append((a,a+1,a+3,a+2))
        uvs.extend((side,v[2]/tip) for i,v in enumerate(blade.vertices) for side in (i%2,) )
        colors.extend((v[2],blade.tone,blade.patch,1) for v in blade.vertices)
    maximum = max(v[2] for v in vertices)
    colors = [(r/maximum,g,b,a) for r,g,b,a in colors]
    return vertices,faces,uvs,colors
