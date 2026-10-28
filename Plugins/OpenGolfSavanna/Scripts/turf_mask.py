# SPDX-License-Identifier: MIT
"""Bake the course surface field: signed distances to the play outlines for the turf shader.

Each RGBA channel holds one surface's signed distance in metres (negative inside), clamped to
its range and stored as 0.5 + d / (2 * range). Bilinear filtering of a distance field keeps
straight and curved edges smooth between texels. The ground and blade materials read the same
field, so a seam between two ground materials shows no change of colour. A JSON sidecar
carries the bounds and ranges, so Unreal's Python never needs NumPy.
"""
import argparse
import json
from pathlib import Path
import struct

import numpy as np

# Channel order and range in metres. Fairway reaches across the first-cut band.
CHANNELS = (('FAIRWAY', 2.5), ('GREEN', 1.5), ('FRINGE', 1.5), ('SAND', 1.5))
# Later surfaces are drawn over earlier ones, matching the terrain's last-region-wins rule.
LAYER_ORDER = ('ROUGH', 'FAIRWAY', 'FRINGE', 'GREEN', 'SAND')
SIZE = (4096, 1024)


def polygon_fields(points, x, y):
    inside = np.zeros(np.broadcast_shapes(x.shape, y.shape), dtype=bool)
    distance = np.full(inside.shape, np.inf)
    for a, b in zip(points, points[1:] + points[:1]):
        dx, dy = b[0]-a[0], b[1]-a[1]
        t = np.clip(((x-a[0])*dx + (y-a[1])*dy) / max(dx*dx+dy*dy, 1e-12), 0, 1)
        distance = np.minimum(distance, np.hypot(x-a[0]-t*dx, y-a[1]-t*dy))
        if abs(dy) > 1e-12:
            inside ^= ((a[1] > y) != (b[1] > y)) & (x < dx*(y-a[1])/dy+a[0])
    return inside | (distance < 1e-5), distance


def check_layer_order(spec):
    ranks = [LAYER_ORDER.index(r['surface']) for r in spec['regions'] if r['surface'] in LAYER_ORDER]
    if ranks != sorted(ranks):
        raise ValueError('Regions must run rough, fairway, fringe, green, sand so the shader layers match play')


def field_bounds(spec, margin=8.0):
    """Every non-rough outline plus a margin, in terrain metres: ((min x, min y), (max x, max y))."""
    points = [p for r in spec['regions'] if r['surface'] != 'ROUGH' for p in r['outline']]
    if not points:
        raise ValueError('The course needs at least one play outline')
    low = tuple(float(min(p[i] for p in points) - margin) for i in range(2))
    high = tuple(float(max(p[i] for p in points) + margin) for i in range(2))
    return low, high


def _signed_rows(spec, x, y):
    signed = {surface: np.full(np.broadcast_shapes(x.shape, y.shape), np.inf) for surface, _ in CHANNELS}
    for region in spec['regions']:
        if region['surface'] in signed:
            inside, edge = polygon_fields(region['outline'], x, y)
            signed[region['surface']] = np.minimum(signed[region['surface']], np.where(inside, -edge, edge))
    return signed


def field_channels(spec, bounds, width=SIZE[0], height=SIZE[1]):
    """RGBA8 rows, top row at the minimum y."""
    check_layer_order(spec)
    (x0, y0), (x1, y1) = bounds
    x = x0 + (np.arange(width)[None, :] + 0.5) * (x1 - x0) / width
    result = np.empty((height, width, 4), dtype=np.uint8)
    for start in range(0, height, 32):
        end = min(height, start + 32)
        y = y0 + (np.arange(start, end)[:, None] + 0.5) * (y1 - y0) / height
        signed = _signed_rows(spec, x, y)
        for channel, (surface, limit) in enumerate(CHANNELS):
            result[start:end, :, channel] = np.rint(np.clip(.5 + signed[surface] / (2 * limit), 0, 1) * 255).astype(np.uint8)
    return result


def decode(texel):
    """Distances in metres from one RGBA texel."""
    return {surface: (int(texel[i]) / 255 - .5) * 2 * limit for i, (surface, limit) in enumerate(CHANNELS)}


def write_field(spec, output, width=SIZE[0], height=SIZE[1]):
    """Write a 32-bit top-left Targa and its JSON sidecar; returns the sidecar contents."""
    bounds = field_bounds(spec)
    data = field_channels(spec, bounds, width, height)
    header = struct.pack('<BBBHHBHHHHBB', 0, 0, 2, 0, 0, 0, 0, 0, width, height, 32, 0x28)
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_bytes(header + data[:, :, [2, 1, 0, 3]].tobytes())
    info = {'minimum': list(bounds[0]), 'maximum': list(bounds[1]), 'size': [width, height],
            'channels': [[surface, limit] for surface, limit in CHANNELS]}
    output.with_suffix('.json').write_text(json.dumps(info, indent=2) + '\n')
    return info


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('definition', type=Path, help='Course outlines exported by ApplyCutTurf.py --export-mask')
    parser.add_argument('output', type=Path, help='Targa to write, for example T_CourseSurfaceField.tga')
    args = parser.parse_args()
    write_field(json.loads(args.definition.read_text()), args.output)
