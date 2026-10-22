# SPDX-License-Identifier: MIT
"""Populate the public range map without replacing existing layout actors or edits."""
import unreal

MAP = "/Game/UOpenGolf/Maps/UOpenGolf"
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
if not levels.load_level(MAP):
    raise RuntimeError(f"Could not load {MAP}")
ranges = [actor for actor in actors.get_all_level_actors() if isinstance(actor, unreal.UOpenGolfRange)]
if len(ranges) > 1:
    raise RuntimeError("The map contains multiple range controllers; no actors were changed")
range_actor = ranges[0] if ranges else actors.spawn_actor_from_class(unreal.UOpenGolfRange, unreal.Vector())
range_actor.set_actor_label("Range settings")
range_actor.set_folder_path("Range")
range_actor.build_default_layout()
range_actor.refresh_presentation()
unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).set_level_viewport_camera_info(
    unreal.Vector(-600, 0, 220), unreal.Rotator(-2, 0, 0))
if not levels.save_current_level():
    raise RuntimeError("Could not save the editable range")
unreal.log("OPEN_GOLF_RANGE_MAP_COMPLETE")
