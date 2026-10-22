# SPDX-License-Identifier: MIT
"""Play short ball shots into one saved jacaranda: wood must rebound, the canopy must slow without rebounding.

    py "<project>/Plugins/OpenGolfSavanna/Scripts/PlayTreeShots.py"

Run it once with Play stopped (it finds the probe faces, which the editor's mesh queries refuse
during Play, and caches them in Saved/Automation/SavannaRange/TreeShotFaces.json), then start Play
on the savanna course and run it again. Three shots with no wind: a slow one at the low trunk and
one at a mid-height trunk face, both from 1 m outside the tree (they must come back toward the start),
and a 40 mph one from 10 m away sideways across an outer branch face (it must not come back, and with
canopy volumes applied it must lose at least 40% of its speed within a second). Results go to
Saved/Automation/SavannaRange/TreeShots.json and are logged as OPEN_GOLF_TREE_SHOTS_COMPLETE.
"""
from pathlib import Path
import importlib
import json
import math
import sys
import time

import unreal as ue

sys.path.insert(0, str(Path(__file__).resolve().parent))
import savanna_trees as trees

trees = importlib.reload(trees)
wood = trees.wood
# Trunk shots start close so the slow one reaches the wood before it drops; the canopy shot crosses the crown.
WOOD_START_CM = 100
CANOPY_START_CM = 1000
SETTLE_S = 1.5


def low_trunk_face(mesh):
    """A sideways face of the trunk about a metre above the ground."""
    section = next(s for lod, s, slot in wood._section_slots(mesh) if lod == 0 and slot in trees.WOOD)
    vertices, triangles, *_ = ue.ProceduralMeshLibrary.get_section_from_static_mesh(mesh, 0, section)
    for offset in range(0, len(triangles), 3):
        a, b, c = (vertices[triangles[offset + i]] for i in range(3))
        center = (a + b + c) / 3
        normal = ue.MathLibrary.cross_vector_vector(b - a, c - a)
        if 60 < center.z < 140 and normal.length() > 2 and abs(normal.z / normal.length()) < 0.3:
            return "low trunk", center, normal / normal.length(), True
    raise RuntimeError("Could not find a low trunk face")


FACES = Path(ue.Paths.project_saved_dir()) / "Automation/SavannaRange/TreeShotFaces.json"
world = ue.get_editor_subsystem(ue.UnrealEditorSubsystem).get_game_world()
if not world:
    proxy = ue.load_asset(trees.PROXY)
    found = [low_trunk_face(proxy)] + wood.probe_faces(proxy, trees.WOOD, ue.load_asset(trees.SOURCE), {"jacaranda_tree_branches"})
    FACES.parent.mkdir(parents=True, exist_ok=True)
    FACES.write_text(json.dumps([[name, [p.x, p.y, p.z], [n.x, n.y, n.z], hit] for name, p, n, hit in found]))
    ue.log(f"OPEN_GOLF_TREE_SHOTS_PREPARED {len(found)} faces; start Play and run this script again")
    raise SystemExit(0)
if not FACES.exists():
    raise RuntimeError("Run this script once with Play stopped to find the probe faces")
faces = [(name, ue.Vector(*p), ue.Vector(*n), hit) for name, p, n, hit in json.loads(FACES.read_text())]
actors = ue.GameplayStatics.get_all_actors_of_class(world, ue.Actor)
tree_transforms, _ = wood.tree_data(actors, trees.SOURCE)
transform = next(iter(tree_transforms.values()))
canopy_applied = any(actor.actor_has_tag(trees.CANOPY_TAG) for actor in actors)
ball = ue.GameplayStatics.get_all_actors_of_class(world, ue.OpenFairwayBall)[0]
original = (ball.get_actor_location(), ball.get_editor_property("aim_yaw_deg"), ball.get_editor_property("wind_speed_mph"),
            ball.get_editor_property("draw_tracer"))
ball.set_editor_property("wind_speed_mph", 0)
results, state = [], {}


def restore():
    ue.unregister_slate_post_tick_callback(handle)
    ball.place_at(original[0])
    ball.set_editor_property("aim_yaw_deg", original[1])
    ball.set_editor_property("wind_speed_mph", original[2])
    ball.set_editor_property("draw_tracer", original[3])


def launch(index):
    name, point, normal, wood_face = faces[index]
    radial = ue.Vector(point.x, point.y, 0)
    radial = radial / radial.length() if radial.length() > 1 else ue.Vector(1, 0, 0)
    # Wood: come in from outside, toward the trunk axis. Canopy: cross the outer crown sideways, so the
    # slowed ball never reaches the trunk or main limbs (they end 4.5 m out; this face is further out).
    approach = radial if wood_face else ue.Vector(-radial.y, radial.x, 0)
    target = ue.MathLibrary.transform_location(transform, point)
    start = ue.MathLibrary.transform_location(transform, point + approach * (WOOD_START_CM if wood_face else CANOPY_START_CM))
    direction = (target - start) / (target - start).length()
    ball.set_editor_property("aim_yaw_deg", math.degrees(math.atan2(direction.y, direction.x)))
    shot = ue.OpenFairwayLaunch()
    shot.ball_speed_mph = 12 if name == "low trunk" else 40
    shot.vertical_launch_deg = math.degrees(math.asin(max(-1.0, min(1.0, direction.z))))
    shot.back_spin_rpm = 0
    if not ball.launch(shot):
        raise RuntimeError("The ball rejected a tree shot launch")
    ball.set_actor_location(start, False, True)  # launch snaps to the ground; start this shot in the air
    state.update(index=index, name=name, wood=wood_face, start=start, direction=direction, previous=start, reflected=False,
                 started=time.monotonic(), launch_mps=shot.ball_speed_mph * 0.44704, speed_1s=None)


def tick(_delta):
    try:
        now = ball.get_actor_location()
        if ue.MathLibrary.dot_vector_vector(now - state["previous"], state["direction"]) < -0.05:
            state["reflected"] = True
        state["previous"] = now
        elapsed = time.monotonic() - state["started"]
        if state["speed_1s"] is None and elapsed >= 1.0:
            state["speed_1s"] = ball.get_speed()
        if elapsed < SETTLE_S:
            return
        kept = state["speed_1s"] / state["launch_mps"]
        if state["wood"]:
            passed = state["reflected"]
        else:
            passed = not state["reflected"] and (kept < 0.6 or not canopy_applied)
        results.append({"part": state["name"], "wood": state["wood"], "reflected": state["reflected"],
                        "speed_kept_after_1s": round(kept, 3), "passed": passed})
        if state["index"] + 1 < len(faces):
            launch(state["index"] + 1)
            return
        restore()
        report = {"canopy_applied": canopy_applied, "shots": results, "passed": all(r["passed"] for r in results)}
        out = Path(ue.Paths.project_saved_dir()) / "Automation/SavannaRange/TreeShots.json"
        out.parent.mkdir(parents=True, exist_ok=True)
        out.write_text(json.dumps(report, indent=2))
        ue.log(f"OPEN_GOLF_TREE_SHOTS_COMPLETE {json.dumps(report)}")
        if not report["passed"]:
            ue.log_error("OPEN_GOLF_TREE_SHOTS_FAILED")
    except Exception as error:
        restore()
        ue.log_error(f"OPEN_GOLF_TREE_SHOTS_FAILED {error}")


launch(0)
handle = ue.register_slate_post_tick_callback(tick)
