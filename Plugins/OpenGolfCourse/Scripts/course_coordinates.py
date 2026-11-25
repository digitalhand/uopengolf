# SPDX-License-Identifier: MIT
"""Coordinates shared by geographic course imports and editor corrections."""

GRID_FILE = "ground_east_south.oge"
AXES_TAG = "OpenGolfCourseAxes"
EAST_SOUTH = "EastSouth"


def unreal_xy(point, units_per_metre=1):
    east, north = point
    return east * units_per_metre, -north * units_per_metre


def source_xy(vector, units_per_metre, east_south):
    north = -vector.y if east_south else vector.y
    return [round(vector.x / units_per_metre, 3), round(north / units_per_metre, 3)]
