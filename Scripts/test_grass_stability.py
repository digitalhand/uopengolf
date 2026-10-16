# SPDX-License-Identifier: MIT
"""Engine-free checks for the stationary-frame grass stability measurement."""
from pathlib import Path
import tempfile
import unittest

from PIL import Image

from grass_stability import compare_frames, compare_sequence, find_sequence, latest_sequence


class GrassStabilityTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)

    def image(self, name, size=(10, 10), color=(40, 100, 30), block=None):
        """Solid image; block is (box, color) painted over it."""
        picture = Image.new("RGB", size, color)
        if block:
            picture.paste(block[1], block[0])
        path = self.root / name
        picture.save(path)
        return path

    def test_identical_frames_are_stable(self):
        first, last = self.image("a.png"), self.image("b.png")
        result = compare_frames(first, last, crop_fraction=1.0)
        self.assertEqual(result.changed_percent, 0.0)
        self.assertEqual(result.mean_max_difference, 0.0)

    def test_changed_block_reports_percent_and_mean(self):
        first = self.image("a.png")
        # 20 of 100 pixels change by 10 levels in their largest channel.
        last = self.image("b.png", block=((0, 8, 10, 10), (40, 110, 30)))
        result = compare_frames(first, last, crop_fraction=1.0)
        self.assertAlmostEqual(result.changed_percent, 20.0)
        self.assertAlmostEqual(result.mean_max_difference, 2.0)

    def test_small_differences_are_below_threshold(self):
        first = self.image("a.png")
        last = self.image("b.png", color=(43, 103, 33))
        result = compare_frames(first, last, crop_fraction=1.0)
        self.assertEqual(result.changed_percent, 0.0)
        self.assertAlmostEqual(result.mean_max_difference, 3.0)

    def test_crop_keeps_only_lower_rows(self):
        first = self.image("a.png")
        # The change is in the top half; a lower-40% crop must ignore it.
        last = self.image("b.png", block=((0, 0, 10, 5), (200, 200, 200)))
        self.assertEqual(compare_frames(first, last, crop_fraction=0.4).changed_percent, 0.0)
        self.assertAlmostEqual(compare_frames(first, last, crop_fraction=1.0).changed_percent, 50.0)

    def test_rejects_mismatched_sizes_and_bad_crop(self):
        first, last = self.image("a.png"), self.image("b.png", size=(12, 10))
        with self.assertRaises(ValueError):
            compare_frames(first, last)
        with self.assertRaises(ValueError):
            compare_frames(first, first, crop_fraction=0.0)

    def test_find_sequence_returns_frames_in_order(self):
        for frame in (20, 1, 2):
            self.image(f"20261016-010000-View0-Frame{frame:02d}.png")
        self.image("20261016-010000-View1-Frame01.png")
        frames = find_sequence(self.root, "20261016-010000", 0)
        self.assertEqual([f.name[-11:] for f in frames], ["Frame01.png", "Frame02.png", "Frame20.png"])
        with self.assertRaises(FileNotFoundError):
            find_sequence(self.root, "20261016-010000", 4)

    def test_sequence_averages_consecutive_pairs(self):
        # Frame 2 differs from frames 1 and 3 in 20% of pixels; first-vs-last sees nothing.
        frames = [self.image("f1.png"),
                  self.image("f2.png", block=((0, 8, 10, 10), (40, 110, 30))),
                  self.image("f3.png")]
        result = compare_sequence(frames, crop_fraction=1.0)
        self.assertAlmostEqual(result.changed_percent, 20.0)
        self.assertAlmostEqual(result.mean_max_difference, 2.0)
        self.assertEqual(compare_frames(frames[0], frames[-1], crop_fraction=1.0).changed_percent, 0.0)
        with self.assertRaises(ValueError):
            compare_sequence(frames[:1])

    def test_latest_sequence_uses_newest_id_with_one_based_frames(self):
        # The capture numbers frames from 01 (RangePresentationTest increments first).
        for sequence in ("20261016-010000", "20261016-020000"):
            for frame in (1, 20):
                self.image(f"{sequence}-View0-Frame{frame:02d}.png")
        self.image("20261016-030000-TeeGrassOn.png")
        self.assertEqual(latest_sequence(self.root), "20261016-020000")
        with self.assertRaises(FileNotFoundError):
            latest_sequence(self.root / "missing")


if __name__ == "__main__":
    unittest.main()
