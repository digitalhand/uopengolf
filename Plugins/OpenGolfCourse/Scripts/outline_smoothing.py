# SPDX-License-Identifier: MIT
"""Smooth closed course outlines into curves that still pass through every authored corner.

A centripetal Catmull-Rom curve joins the corners without loops or cusps. It is sampled
densely enough that the terrain's constrained edges, the physical surfaces and the turf
shading all follow the same curve. Lengths are in metres.
"""
import math

ALPHA = 0.5  # Centripetal: no cusps or self-intersections within a segment.


def _knot(t, a, b):
    return t + max(math.dist(a, b), 1e-9) ** ALPHA


def _lerp(a, b, t0, t1, t):
    w = (t - t0) / (t1 - t0)
    return (a[0] + (b[0] - a[0]) * w, a[1] + (b[1] - a[1]) * w)


def _point(p0, p1, p2, p3, t):
    """Barry-Goldman evaluation between p1 (t=t1) and p2 (t=t2)."""
    t0 = 0.0
    t1 = _knot(t0, p0, p1)
    t2 = _knot(t1, p1, p2)
    t3 = _knot(t2, p2, p3)
    t = t1 + (t2 - t1) * t
    a1, a2, a3 = _lerp(p0, p1, t0, t1, t), _lerp(p1, p2, t1, t2, t), _lerp(p2, p3, t2, t3, t)
    return _lerp(_lerp(a1, a2, t0, t2, t), _lerp(a2, a3, t1, t3, t), t1, t2, t)


def _validate(points, spacing):
    if len(points) < 3:
        raise ValueError('A closed outline needs at least three points')
    if not math.isfinite(spacing) or spacing <= 0:
        raise ValueError('Outline spacing must be positive')
    for a, b in zip(points, points[1:] + points[:1]):
        if not all(math.isfinite(v) for v in a) or math.dist(a, b) < 0.001:
            raise ValueError('Outline points must be finite and distinct')


def _segment(p0, p1, p2, p3, spacing):
    """Points from p1 towards p2 (p2 excluded), evenly spaced along the curve."""
    ts = [s / 64 for s in range(65)]
    probe = [_point(p0, p1, p2, p3, t) for t in ts]
    lengths = [0.0]
    for a, b in zip(probe, probe[1:]):
        lengths.append(lengths[-1] + math.dist(a, b))
    steps = max(1, math.ceil(lengths[-1] * 1.02 / spacing))
    points, k = [p1], 0
    for s in range(1, steps):
        target = lengths[-1] * s / steps
        while lengths[k + 1] < target:
            k += 1
        w = (target - lengths[k]) / max(lengths[k + 1] - lengths[k], 1e-12)
        points.append(_point(p0, p1, p2, p3, ts[k] + (ts[k + 1] - ts[k]) * w))
    return points


def smooth_outline(points, spacing):
    """Closed curve through every point, sampled no further than spacing apart."""
    points = [(float(x), float(y)) for x, y in points]
    _validate(points, spacing)
    count = len(points)
    result = []
    for i in range(count):
        result.extend(_segment(*(points[(i + k) % count] for k in (-1, 0, 1, 2)), spacing))
    return result


def _cross(o, a, b):
    return (a[0] - o[0]) * (b[1] - o[1]) - (a[1] - o[1]) * (b[0] - o[0])


def _segments_cross(a, b, c, d):
    """Proper crossings only: shared endpoints and touching do not count, as in the terrain check."""
    return _cross(a, b, c) * _cross(a, b, d) < 0 and _cross(c, d, a) * _cross(c, d, b) < 0


def _edges(outline, index):
    return [(index, k, a, b) for k, (a, b) in enumerate(zip(outline, outline[1:] + outline[:1]))]


def _cells(a, b, size):
    x0, x1 = sorted((math.floor(a[0] / size), math.floor(b[0] / size)))
    y0, y1 = sorted((math.floor(a[1] / size), math.floor(b[1] / size)))
    return [(x, y) for x in range(x0, x1 + 1) for y in range(y0, y1 + 1)]


def crossing_outlines(outlines):
    """Sorted index pairs (i, j), i <= j, of outlines whose edges cross; (i, i) is a self-crossing."""
    edges = [e for i, outline in enumerate(outlines) for e in _edges(outline, i)]
    size = max(max(math.dist(a, b) for _, _, a, b in edges), 1e-6)
    grid = {}
    for edge in edges:
        for cell in _cells(edge[2], edge[3], size):
            grid.setdefault(cell, []).append(edge)
    found = set()
    for bucket in grid.values():
        for n, (i, k, a, b) in enumerate(bucket):
            for j, m, c, d in bucket[n + 1:]:
                if (i, k) != (j, m) and _segments_cross(a, b, c, d):
                    found.add((min(i, j), max(i, j)))
    return sorted(found)
