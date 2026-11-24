# SPDX-License-Identifier: MIT
"""Time limit for editor work that finishes over several frames, such as baking placed vegetation.

The clock adds up frame times, each capped, instead of reading the wall clock. Time the editor spends stopped in a
debugger, or inside one long blocking frame, then counts as a single short frame and cannot exhaust the limit.
Pure Python, so it can be tested without Unreal.
"""


class BakeClock:
    def __init__(self, limit_s, max_frame_s=1.0):
        if limit_s <= 0 or max_frame_s <= 0:
            raise ValueError('A bake clock needs a positive limit and frame cap')
        self.limit_s = limit_s
        self.max_frame_s = max_frame_s
        self.elapsed_s = 0.0

    def tick(self, delta_s):
        """Count one frame of delta_s seconds."""
        self.elapsed_s += min(max(delta_s, 0.0), self.max_frame_s)

    @property
    def expired(self):
        return self.elapsed_s >= self.limit_s
