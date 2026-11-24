# SPDX-License-Identifier: MIT
"""Engine-free checks for the training-ring stability measurement."""
import json
from pathlib import Path
import tempfile
import unittest

from PIL import Image, ImageDraw

from Assets.Scripts.ring_stability import arc_columns, arc_presence, continuity, find_views, measure_view, toggle_percent

GRASS = (60, 110, 40)
WHITE = (235, 235, 235)


class RingStabilityTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)

    def frame(self, name, size=(40, 20), dashes=None, row=8):
        """Grass with a white horizontal arc on row; dashes=(on, off, phase) leaves gaps."""
        picture = Image.new("RGB", size, GRASS)
        draw = ImageDraw.Draw(picture)
        if dashes is not False:
            for x in range(5, 35):
                if dashes and (x + dashes[2]) % (dashes[0] + dashes[1]) >= dashes[0]:
                    continue
                draw.point((x, row), WHITE)
        path = self.root / name
        picture.save(path)
        return path

    def sidecar(self, view, size=(40, 20), row=8):
        width, height = size
        data = {"view": view, "box": [0.1, 0.2, 0.9, 0.8],
                "far_arc": [[5 / width, row / height], [34 / width, row / height]]}
        (self.root / f"{view}-ring.json").write_text(json.dumps(data))

    def test_columns_follow_the_arc_polyline(self):
        columns = arc_columns([(2.0, 4.0), (6.0, 8.0)])
        self.assertEqual([x for x, _ in columns], [2, 3, 4, 5, 6])
        self.assertEqual([y for _, y in columns], [4.0, 5.0, 6.0, 7.0, 8.0])

    def test_columns_accept_right_to_left_points(self):
        self.assertEqual([x for x, _ in arc_columns([(6.0, 1.0), (4.0, 1.0)])], [4, 5, 6])

    def test_solid_arc_is_continuous(self):
        columns = arc_columns([(5.0, 8.0), (34.0, 8.0)])
        presence = arc_presence(self.frame("on.png"), self.frame("off.png", dashes=False), columns)
        self.assertEqual(continuity(presence), 100.0)

    def test_arc_found_within_the_search_radius(self):
        # The projected arc can be a pixel or two off the drawn line.
        columns = arc_columns([(5.0, 10.0), (34.0, 10.0)])
        presence = arc_presence(self.frame("on.png"), self.frame("off.png", dashes=False), columns, radius=2)
        self.assertEqual(continuity(presence), 100.0)
        self.assertEqual(continuity(arc_presence(self.frame("on.png"), self.frame("off.png", dashes=False), columns, radius=1)), 0.0)

    def test_dashed_arc_reports_its_gaps(self):
        columns = arc_columns([(5.0, 8.0), (34.0, 8.0)])
        presence = arc_presence(self.frame("on.png", dashes=(3, 3, 0)), self.frame("off.png", dashes=False), columns)
        self.assertAlmostEqual(continuity(presence), 50.0)

    def test_moving_dashes_toggle_and_steady_dashes_do_not(self):
        columns = arc_columns([(5.0, 8.0), (34.0, 8.0)])
        off = self.frame("off.png", dashes=False)
        steady = [arc_presence(self.frame(f"s{i}.png", dashes=(3, 3, 0)), off, columns) for i in range(3)]
        moving = [arc_presence(self.frame(f"m{i}.png", dashes=(3, 3, 3 * i)), off, columns) for i in range(3)]
        self.assertEqual(toggle_percent(steady), 0.0)
        self.assertEqual(toggle_percent(moving), 100.0)

    def test_measure_view_compares_rings_on_and_hidden(self):
        self.sidecar("tee-15yd")
        for index in range(3):
            self.frame(f"tee-15yd-{index:02d}.png", dashes=(3, 3, 3 * index))
            self.frame(f"tee-15yd-hidden-{index:02d}.png", dashes=False)
        result = measure_view(self.root, "tee-15yd")
        self.assertEqual(result.frames, 3)
        self.assertAlmostEqual(result.continuity_mean, 50.0)
        self.assertAlmostEqual(result.continuity_min, 50.0)
        self.assertEqual(result.toggle_percent, 100.0)
        self.assertGreater(result.shown.sparkle_percent, result.hidden.sparkle_percent)
        self.assertEqual(result.hidden.sparkle_percent, 0.0)

    def test_find_views_lists_sidecars(self):
        self.sidecar("tee-150yd")
        self.sidecar("tee-15yd")
        self.assertEqual(find_views(self.root), ["tee-150yd", "tee-15yd"])

    def test_missing_hidden_frames_fail_clearly(self):
        self.sidecar("tee")
        self.frame("tee-00.png")
        self.frame("tee-01.png")
        with self.assertRaises(FileNotFoundError):
            measure_view(self.root, "tee")

    def test_rejects_mismatched_frame_sizes(self):
        columns = arc_columns([(5.0, 8.0), (34.0, 8.0)])
        with self.assertRaises(ValueError):
            arc_presence(self.frame("on.png"), self.frame("off.png", size=(41, 20), dashes=False), columns)


if __name__ == "__main__":
    unittest.main()
