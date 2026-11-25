# SPDX-License-Identifier: MIT
"""Apply the course kit's stable terrain shading without rebuilding terrain or existing course materials.

A range saved before the tee pad moved into the terrain still has a TeeBox component; it gets the tee material.
"""
from pathlib import Path
import sys
import unreal as ue

sys.path.insert(0, str(Path(__file__).resolve().parent))
import BuildTerrainAssets as builder
import course_assets as assets

previous_owner = assets.OWNER
assets.OWNER = builder.OWNER
try:
    parent = builder.build_material()
    builder.build_grass_material()
    tee = builder.build_tee_material(parent)
    levels = ue.get_editor_subsystem(ue.LevelEditorSubsystem)
    if not levels.load_level('/Game/UOpenGolf/Maps/L_SavannaRange'):
        raise RuntimeError('Could not open the range map')
    actors = ue.get_editor_subsystem(ue.EditorActorSubsystem)
    for actor in actors.get_all_level_actors():
        for component in actor.get_components_by_class(ue.StaticMeshComponent):
            if component.get_name() == 'TeeBox':
                component.set_material(0, tee)
    if not levels.save_current_level():
        raise RuntimeError('Could not save the tee material assignment')
    ue.log('OPEN_GOLF_TERRAIN_STABILITY_APPLIED')
finally:
    assets.OWNER = previous_owner
