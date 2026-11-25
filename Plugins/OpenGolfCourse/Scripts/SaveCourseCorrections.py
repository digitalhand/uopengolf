# SPDX-License-Identifier: MIT
"""Save the tees, cups, aim points and outlines edited in the Course Designer as author corrections.

    py "<project>/Plugins/OpenGolfCourse/Scripts/SaveCourseCorrections.py" --source <source folder> --course <course asset>

The source folder is the one prepare_course_site.py wrote; its recipe.json names the corrections file.
Only differences from the prepared layout are stored, so the file stays small and survives a new import.
The terrain definition is the DA_Ground asset beside the course, as build_course.py creates it.
"""
import argparse
import json
from pathlib import Path
import sys

import unreal as ue

sys.path.insert(0, str(Path(__file__).resolve().parent))
from course_coordinates import AXES_TAG, EAST_SOUTH, source_xy  # noqa: E402
import course_corrections  # noqa: E402

REGION_NAME_SEPARATOR = "|"  # build_course.py names each region "<surface>|<source id>".


def placed_holes(course, east_south):
    for number, hole in enumerate(course.get_editor_property("layout"), start=1):
        yield {"number": number, "cup_m": source_xy(hole.get_editor_property("cup_cm"), 100, east_south),
               "tees_m": {str(t.get_editor_property("set_name")): source_xy(t.get_editor_property("position_cm"), 100, east_south)
                          for t in hole.get_editor_property("tees")},
               "aim_m": [source_xy(p, 100, east_south) for p in hole.get_editor_property("routing_cm")]}


def placed_regions(terrain, east_south):
    for region in terrain.get_editor_property("regions"):
        surface, _, rid = str(region.get_editor_property("name")).partition(REGION_NAME_SEPARATOR)
        if rid:
            yield {"id": rid, "surface": surface,
                   "outline_m": [source_xy(p, 1, east_south) for p in region.get_editor_property("outline_m")]}


def main(argv):
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--source", required=True, type=Path)
    parser.add_argument("--course", required=True)
    args = parser.parse_args(argv)
    recipe = json.loads((args.source / "recipe.json").read_text())
    if not recipe.get("corrections"):
        raise ValueError("The site recipe names no corrections file")
    course = ue.load_asset(args.course)
    terrain = ue.load_asset(args.course.rsplit("/", 1)[0] + "/DA_Ground")
    if not course or not terrain:
        raise ValueError(f"Could not load {args.course} and the DA_Ground beside it")
    layout = json.loads((args.source / "source_layout.json").read_text())
    path = Path(recipe["corrections"])
    previous = json.loads(path.read_text()) if path.is_file() else None
    # Courses built before the axis fix stored north as positive Y.
    east_south = ue.EditorAssetLibrary.get_metadata_tag(course, AXES_TAG) == EAST_SOUTH
    corrections = course_corrections.capture(layout, list(placed_holes(course, east_south)), list(placed_regions(terrain, east_south)), previous)
    course_corrections.apply(layout, corrections)  # Refuse to write a file the build could not read back.
    path.write_text(json.dumps(corrections, indent=2) + "\n", encoding="utf-8")
    ue.log(f"COURSE_CORRECTIONS_SAVED holes={len(corrections['holes'])} regions={len(corrections['regions'])} {path}")


if __name__ == "__main__":
    main(sys.argv[1:])
