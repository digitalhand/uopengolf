# SPDX-License-Identifier: MIT
"""Course-independent hole layout geometry: moving greens and bunkers, Gaussian terrain mounds and green slopes.

Terrain-local metres, X downrange and Y right, as UOpenGolfTerrainDefinition. Mounds are (cx, cy, rx, ry, height)
and add height * exp(-2 |(p - c) / r|^2), the same sum as UOpenGolfTerrainDefinition::HeightAt (without the tee
box flattening, which only applies at the tee). No unreal import, so courses test their layouts under plain Python.
"""
import math


def bounds(outline):
    xs, ys = [p[0] for p in outline], [p[1] for p in outline]
    return min(xs), min(ys), max(xs), max(ys)


def center(outline):
    """Centre of the outline's bounding box."""
    x0, y0, x1, y1 = bounds(outline)
    return (x0 + x1) / 2, (y0 + y1) / 2


def translate(outline, dx, dy):
    return [(x + dx, y + dy) for x, y in outline]


def scale_about(outline, factor, origin):
    ox, oy = origin
    return [(ox + (x - ox) * factor, oy + (y - oy) * factor) for x, y in outline]


def trim_end(outline, start_x, end_x):
    """The outline with its downrange end pulled back to end_x: points past start_x move toward it in proportion, so the
    end keeps its shape. An outline already ending at end_x is unchanged."""
    reach = bounds(outline)[2]
    if reach <= start_x:
        return list(outline)
    factor = (end_x - start_x) / (reach - start_x)
    return [(start_x + (x - start_x) * factor if x > start_x else x, y) for x, y in outline]


def mounds_at(mounds, point, tolerance=0.1):
    """The mounds centred on point, such as a bunker's lip and hollow."""
    return [m for m in mounds if math.hypot(m[0] - point[0], m[1] - point[1]) < tolerance]


def move_mounds(mounds, origin, target, factor=1.0):
    """Mounds centred at origin moved to target, their radii and heights scaled by factor; the rest unchanged."""
    moved = []
    for m in mounds:
        if math.hypot(m[0] - origin[0], m[1] - origin[1]) < 0.1:
            moved.append((target[0], target[1], m[2] * factor, m[3] * factor, m[4] * factor))
        else:
            moved.append(m)
    return moved


def copy_feature(outline, mounds, origin, target, factor=1.0):
    """A copy of a feature (outline plus the mounds centred at origin) moved from origin to target and scaled by factor
    about it: (outline, new mounds)."""
    copied = translate(scale_about(outline, factor, origin), target[0] - origin[0], target[1] - origin[1])
    return copied, [(target[0], target[1], m[2] * factor, m[3] * factor, m[4] * factor) for m in mounds_at(mounds, origin)]


def height_at(mounds, x, y):
    height = 0.0
    for cx, cy, rx, ry, h in mounds:
        if rx > 0 and ry > 0:
            height += h * math.exp(-2.0 * (((x - cx) / rx) ** 2 + ((y - cy) / ry) ** 2))
    return height


def slope_at(mounds, x, y, step=0.02):
    """Grade (rise over run) at a point: the size of the height gradient."""
    gx = (height_at(mounds, x + step, y) - height_at(mounds, x - step, y)) / (2 * step)
    gy = (height_at(mounds, x, y + step) - height_at(mounds, x, y - step)) / (2 * step)
    return math.hypot(gx, gy)


def contains(outline, point):
    """Even-odd point in polygon."""
    x, y = point
    inside = False
    for (ax, ay), (bx, by) in zip(outline, outline[1:] + outline[:1]):
        if (ay > y) != (by > y) and x < ax + (y - ay) * (bx - ax) / (by - ay):
            inside = not inside
    return inside


def max_slope_inside(mounds, outline, spacing=0.5):
    """Steepest grade on a grid over the outline's inside, and where it is: (grade, (x, y))."""
    x0, y0, x1, y1 = bounds(outline)
    worst = (0.0, None)
    y = y0
    while y <= y1:
        x = x0
        while x <= x1:
            if contains(outline, (x, y)):
                grade = slope_at(mounds, x, y)
                if grade > worst[0]:
                    worst = (grade, (x, y))
            x += spacing
        y += spacing
    return worst


def max_slope_near(mounds, point, radius, spacing=0.25):
    """Steepest grade within radius of point, such as around a cup."""
    worst = 0.0
    steps = int(radius / spacing)
    for i in range(-steps, steps + 1):
        for j in range(-steps, steps + 1):
            dx, dy = i * spacing, j * spacing
            if dx * dx + dy * dy <= radius * radius:
                worst = max(worst, slope_at(mounds, point[0] + dx, point[1] + dy))
    return worst


def outline_distance(a, b):
    """Smallest distance between two outlines' edges; 0 when they cross or one holds the other's vertex."""
    if any(contains(b, p) for p in a) or any(contains(a, p) for p in b):
        return 0.0

    def segment_distance(p, s, e):
        dx, dy = e[0] - s[0], e[1] - s[1]
        length2 = dx * dx + dy * dy
        t = max(0.0, min(1.0, ((p[0] - s[0]) * dx + (p[1] - s[1]) * dy) / length2)) if length2 else 0.0
        return math.hypot(p[0] - s[0] - t * dx, p[1] - s[1] - t * dy)

    edges_a, edges_b = list(zip(a, a[1:] + a[:1])), list(zip(b, b[1:] + b[:1]))
    return min(min(segment_distance(p, s, e) for p in a for s, e in edges_b), min(segment_distance(p, s, e) for p in b for s, e in edges_a))
