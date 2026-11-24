# SPDX-License-Identifier: MIT
import unittest

from bake_clock import BakeClock


class BakeClockTest(unittest.TestCase):
    def test_counts_ordinary_frames(self):
        clock = BakeClock(1.0)
        for _ in range(3):
            clock.tick(.25)
        self.assertFalse(clock.expired)
        clock.tick(.25)
        self.assertTrue(clock.expired)

    def test_a_debugger_pause_counts_as_one_frame(self):
        # A frame that arrives after eight minutes stopped in gdb must not time the bake out.
        clock = BakeClock(300)
        clock.tick(480)
        self.assertFalse(clock.expired)
        self.assertEqual(clock.elapsed_s, 1.0)

    def test_ignores_negative_deltas(self):
        clock = BakeClock(1.0)
        clock.tick(-5)
        self.assertEqual(clock.elapsed_s, 0)

    def test_rejects_invalid_limits(self):
        with self.assertRaises(ValueError):
            BakeClock(0)
        with self.assertRaises(ValueError):
            BakeClock(10, max_frame_s=0)


if __name__ == '__main__':
    unittest.main()
