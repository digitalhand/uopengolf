# SPDX-License-Identifier: MIT
"""The parts of a placeable tree actor, as the ball physics needs them. Pure Python, so it can be tested without Unreal.

The visible tree never touches the ball. A hidden wood mesh (trunk and main limbs) blocks it as an obstacle, and a
hidden canopy hull slows it without blocking (OpenFairwayBall.h). The tags go on the components: an actor tag would
apply to all of them and make the canopy an obstacle. These match the vegetation builder's generated trees
(OpenGolfTerrainVegetation.cpp) and Docs/CourseDesign/course_vegetation.md.
"""
import argparse
import re

OBSTACLE_TAG = "OpenFairway.Obstacle"
CANOPY_TAG = "OpenFairway.Canopy"
# Trees hold still beyond this, so their distant shadows stay cached (vegetation_wind_assignments.py).
WIND_DISTANCE_CM = 15000


def components(tree, wood, canopy, wind_distance_cm=WIND_DISTANCE_CM):
    """Settings for each static mesh component, in order."""
    common = dict(mobility="STATIC", affects_navigation=False, evaluate_wpo=False, wpo_disable_distance=0, overlap_events=False)
    return [
        {**common, "name": "Tree", "mesh": tree, "profile": "NoCollision", "visible": True, "hidden_in_game": False,
         "cast_shadow": True, "tags": [], "evaluate_wpo": True, "wpo_disable_distance": wind_distance_cm},
        {**common, "name": "Wood", "mesh": wood, "profile": "BlockAll", "visible": False, "hidden_in_game": True,
         "cast_shadow": False, "tags": [OBSTACLE_TAG]},
        {**common, "name": "Canopy", "mesh": canopy, "profile": "OverlapAll", "visible": False, "hidden_in_game": True,
         "cast_shadow": False, "tags": [CANOPY_TAG]},
    ]


def _content_path(value):
    if not re.fullmatch(r"(/[A-Za-z0-9_]+)+", value):
        raise argparse.ArgumentTypeError(f"not a content path: {value!r}")
    return value


def parse_args(argv):
    parser = argparse.ArgumentParser(description="Build or update a placeable tree Blueprint.")
    for name in ("tree", "wood", "canopy", "blueprint"):
        parser.add_argument(f"--{name}", required=True, type=_content_path)
    parser.add_argument("--wind-distance-cm", type=int, default=WIND_DISTANCE_CM)
    return parser.parse_args(argv)
