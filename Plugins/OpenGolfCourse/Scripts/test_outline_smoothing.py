# SPDX-License-Identifier: MIT
import math
import unittest

from outline_smoothing import crossing_outlines, smooth_outline

# The range's original authored fairway (from the retired UpgradeRangeTerrain.py): 17 corners joined by straight edges.
FAIRWAY = [(-12, -13), (25, -16), (75, -14), (120, -17), (165, -17), (220, -14), (285, -16), (335, -11), (350, 0),
           (335, 12), (280, 15), (220, 17), (160, 16), (110, 15), (60, 17), (10, 14), (-12, 12)]


def ellipse(cx, cy, rx, ry, count=48, variation=0.07):
    angles = (math.tau * i / count for i in range(count))
    return [(cx + rx * (1 + variation * math.sin(3 * a)) * math.cos(a), cy + ry * (1 + variation * math.sin(3 * a)) * math.sin(a))
            for a in angles]


def segment_distance(p, a, b):
    dx, dy = b[0] - a[0], b[1] - a[1]
    t = max(0, min(1, ((p[0] - a[0]) * dx + (p[1] - a[1]) * dy) / (dx * dx + dy * dy)))
    return math.hypot(p[0] - a[0] - t * dx, p[1] - a[1] - t * dy)


def gap(inner, outer):
    """Smallest distance from any inner vertex to the outer outline."""
    edges = list(zip(outer, outer[1:] + outer[:1]))
    return min(segment_distance(p, a, b) for p in inner for a, b in edges)


def turning(outline):
    """Largest change of heading between neighbouring edges, in degrees."""
    headings = [math.atan2(b[1] - a[1], b[0] - a[0]) for a, b in zip(outline, outline[1:] + outline[:1])]
    return max(abs(math.degrees((b - a + math.pi) % math.tau - math.pi)) for a, b in zip(headings, headings[1:] + headings[:1]))


class OutlineSmoothingTest(unittest.TestCase):
    def test_passes_through_every_authored_corner(self):
        smooth = smooth_outline(FAIRWAY, 1)
        for corner in FAIRWAY:
            self.assertTrue(any(math.dist(corner, p) < 1e-9 for p in smooth), corner)

    def test_samples_are_close_together_and_distinct(self):
        smooth = smooth_outline(FAIRWAY, 1)
        steps = [math.dist(a, b) for a, b in zip(smooth, smooth[1:] + smooth[:1])]
        self.assertLessEqual(max(steps), 1 + 1e-6)
        self.assertGreater(min(steps), .001)

    def test_curves_instead_of_corners(self):
        # A corner keeps its full turn however finely it is sampled; a curve's turn shrinks with the step.
        self.assertGreater(turning(FAIRWAY), 20)
        coarse, fine = turning(smooth_outline(FAIRWAY, 1)), turning(smooth_outline(FAIRWAY, .25))
        self.assertLess(coarse, turning(FAIRWAY) / 4)
        self.assertLess(fine, coarse / 3)

    def test_fringe_still_holds_the_green(self):
        fringe, green = ellipse(137.16, 0, 16, 12), ellipse(137.16, 0, 14, 10)
        smooth_fringe, smooth_green = smooth_outline(fringe, .5), smooth_outline(green, .5)
        self.assertGreaterEqual(gap(smooth_green, smooth_fringe), gap(green, fringe) - .05)
        self.assertEqual(crossing_outlines([smooth_fringe, smooth_green]), [])

    def test_smooth_fairway_does_not_cross_itself(self):
        self.assertEqual(crossing_outlines([smooth_outline(FAIRWAY, 1)]), [])

    def test_crossing_outlines_are_reported(self):
        square = [(0, 0), (2, 0), (2, 2), (0, 2)]
        shifted = [(1, 1), (3, 1), (3, 3), (1, 3)]
        self.assertEqual(crossing_outlines([square, shifted]), [(0, 1)])

    def test_repeatable(self):
        self.assertEqual(smooth_outline(FAIRWAY, 1), smooth_outline(FAIRWAY, 1))

    def test_rejects_bad_input(self):
        with self.assertRaises(ValueError):
            smooth_outline([(0, 0), (1, 0)], .5)
        with self.assertRaises(ValueError):
            smooth_outline(FAIRWAY, 0)
        with self.assertRaises(ValueError):
            smooth_outline([(0, 0), (1, 0), (1, 0), (0, 1)], .5)


if __name__ == '__main__':
    unittest.main()
