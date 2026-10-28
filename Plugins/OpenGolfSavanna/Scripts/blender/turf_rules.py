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


# Short cuts are drawn only near the camera over matching ground, so fewer, wider, single-quad blades
# resolve better than dense sub-pixel ones. The green family is baked into a texture only.
SURFACES = {
    'Fairway': Turf(.015, .0040, .0132, '496B32', (2, 4), 1.5, 1),
    'FirstCut': Turf(.030, .0038, .0166, '405E2D', (3, 5), 1.2),
    'Collar': Turf(.020, .0044, .0134, '52733A', (2, 4), 1.5, 1),
    'Green': Turf(.004, .0010, .0045, '638345', (3, 5), .25),
}


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
        height = spec.height * max(.7,min(1.3,rng.uniform(.74,1.25)*(.9+.2*patch)))
        width = spec.width*rng.uniform(.65,1.4)
        lean = height*rng.uniform(.35,.95)
        lx,ly = math.cos(direction)*lean,math.sin(direction)*lean
        twist = rng.uniform(-.55,.55)
        vertices = []
        if spec.segments == 1:
            # One tapered quad: widest at the root.
            rows = ((0,0,1.0,direction),(1,height,rng.uniform(.25,.5),direction+twist))
        else:
            middle = rng.uniform(.25,.5)
            rows = ((0,0,.65,direction),(middle,height*.53,1.0,direction+twist*.4),
                    (1,height,rng.uniform(.45,.85),direction+twist))
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
        tip = blade.vertices[-1][2]
        for row in range(len(blade.vertices)//2-1):
            a = start+2*row
            faces.append((a,a+1,a+3,a+2))
        uvs.extend((side,v[2]/tip) for i,v in enumerate(blade.vertices) for side in (i%2,) )
        colors.extend((v[2],blade.tone,blade.patch,1) for v in blade.vertices)
    maximum = max(v[2] for v in vertices)
    colors = [(r/maximum,g,b,a) for r,g,b,a in colors]
    return vertices,faces,uvs,colors
