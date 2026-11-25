# SPDX-License-Identifier: MIT
"""Keep a course's hand-placed actors across a full rebuild with build_course.py.

    py "<project>/Plugins/OpenGolfCourse/Scripts/course_actors.py" --record <file.json> --map /HunterRanch/Course/L_HunterRanch
    py "<project>/Plugins/OpenGolfCourse/Scripts/course_actors.py" --restore <file.json>

A hand-placed actor is one the builder does not make: an instance of a Blueprint class, such as a tree, without the
terrain's generated tag. --record saves each one's class, transform, label and Outliner folder; --restore places them
in the open map (the one build_course.py just made) and saves it. Labels are brought to Docs/ContentNaming.md on the
way, so BP_Tree_IslandTree02_Hole1 in folder Hole_1 comes back as BP_Tree_IslandTree02_Hole01 in Hole_01.
"""
import argparse
import json
from pathlib import Path
import re
import sys

GENERATED_TAG = "OpenGolf.Terrain.Generated"


def normalize_label(label):
    """Two-digit numbers at the end of each segment: Hole1 -> Hole01, Tree_3 -> Tree_03. Spaces become underscores."""
    segments = re.split(r"[\s_]+", label.strip())
    return "_".join(re.sub(r"(?<![0-9])([0-9])$", r"0\1", s) for s in segments if s)


def record_entry(class_path, location, rotation, scale, label, folder):
    """The saved form of one actor; plain data so it round-trips through JSON."""
    folder = "/".join(normalize_label(part) for part in folder.split("/")) if folder and folder != "None" else ""
    return {"class": class_path, "location": list(location), "rotation": list(rotation), "scale": list(scale),
            "label": normalize_label(label), "folder": folder}


def _is_hand_placed(ue, actor):
    generated = any(str(tag) == GENERATED_TAG for tag in actor.tags)
    return isinstance(actor.get_class(), ue.BlueprintGeneratedClass) and not generated


def record(ue, map_path, file):
    if not ue.get_editor_subsystem(ue.LevelEditorSubsystem).load_level(map_path):
        raise RuntimeError(f"Could not load {map_path}")
    entries = []
    for actor in ue.get_editor_subsystem(ue.EditorActorSubsystem).get_all_level_actors():
        if not _is_hand_placed(ue, actor):
            continue
        transform = actor.get_actor_transform()
        location, rotation, scale = transform.translation, transform.rotation.rotator(), transform.scale3d
        entries.append(record_entry(actor.get_class().get_path_name(), (location.x, location.y, location.z),
                                    (rotation.roll, rotation.pitch, rotation.yaw), (scale.x, scale.y, scale.z),
                                    actor.get_actor_label(), str(actor.get_folder_path())))
    Path(file).write_text(json.dumps({"map": map_path, "actors": entries}, indent=2) + "\n")
    ue.log(f"COURSE_ACTORS_RECORDED {len(entries)} {file}")
    return entries


def restore(ue, file):
    entries = json.loads(Path(file).read_text())["actors"]
    actors = ue.get_editor_subsystem(ue.EditorActorSubsystem)
    for entry in entries:
        cls = ue.load_class(None, entry["class"])
        if not cls:
            raise RuntimeError(f"Could not load {entry['class']}")
        actor = actors.spawn_actor_from_class(cls, ue.Vector(*entry["location"]), ue.Rotator(*entry["rotation"]))
        actor.set_actor_scale3d(ue.Vector(*entry["scale"]))
        actor.set_actor_label(entry["label"])
        if entry["folder"]:
            actor.set_folder_path(entry["folder"])
    if not ue.get_editor_subsystem(ue.LevelEditorSubsystem).save_current_level():
        raise RuntimeError("Could not save the map")
    ue.log(f"COURSE_ACTORS_RESTORED {len(entries)}")
    return len(entries)


def main(argv):
    import unreal as ue

    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    action = parser.add_mutually_exclusive_group(required=True)
    action.add_argument("--record", metavar="FILE")
    action.add_argument("--restore", metavar="FILE")
    parser.add_argument("--map", help="Course map to record from")
    args = parser.parse_args(argv)
    if args.record:
        if not args.map:
            parser.error("--record needs --map")
        record(ue, args.map, args.record)
    else:
        restore(ue, args.restore)


if __name__ == "__main__":
    main(sys.argv[1:])
