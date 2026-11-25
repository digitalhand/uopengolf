# SPDX-License-Identifier: MIT
"""Back up and migrate only the current range's legacy lighting. Run inside Unreal."""
from datetime import datetime, timezone
from pathlib import Path
import shutil
import sys
import unreal

sys.path.insert(0, str(Path(__file__).resolve().parent))
from BuildEnvironmentAssets import build


def migrate():
    editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    if editor.get_game_world():
        raise RuntimeError('Stop Play before migrating the environment.')
    world = editor.get_editor_world()
    if world.get_path_name().split('.')[0] != '/Game/UOpenGolf/Maps/L_SavannaRange':
        raise RuntimeError('Open the UOpenGolf map before migrating.')
    all_actors = actors.get_all_level_actors()
    ranges = [a for a in all_actors if isinstance(a, unreal.UOpenGolfRange)]
    if len(ranges) != 1:
        raise RuntimeError('Expected exactly one range controller.')
    environments = [a for a in all_actors if isinstance(a, unreal.OpenGolfEnvironment)]
    if len(environments) > 1:
        raise RuntimeError('Multiple environments; resolve them before migrating.')
    layout = list(ranges[0].get_editor_property('layout_actors'))
    classes = [unreal.DirectionalLightComponent, unreal.SkyAtmosphereComponent, unreal.SkyLightComponent]
    legacy = [a for a in layout if a and any(a.get_component_by_class(c) for c in classes)]
    conflicts = [a for a in all_actors if a not in legacy and a not in environments and any(a.get_component_by_class(c) for c in classes)]
    if conflicts:
        raise RuntimeError('Unrelated lighting found: ' + ', '.join(a.get_actor_label() for a in conflicts))
    if environments and not legacy:
        unreal.log('OPEN_GOLF_ENVIRONMENT_ALREADY_MIGRATED')
        return
    root = Path(unreal.Paths.project_dir())
    backup = root / 'Saved/AssetBackups/Environment' / datetime.now(timezone.utc).strftime('%Y%m%d-%H%M%S')
    backup.mkdir(parents=True, exist_ok=False)
    for file in (root / 'Content/UOpenGolf/Maps').glob('L_SavannaRange*'):
        if file.is_file():
            shutil.copy2(file, backup / file.name)
    # Also preserve current in-memory map edits before replacing any actors.
    if not level.save_current_level():
        raise RuntimeError('Could not save current map; no lighting was replaced.')
    shutil.copy2(root / 'Content/UOpenGolf/Maps/L_SavannaRange.umap', backup / 'L_SavannaRange-live.umap')
    profile = build()
    with unreal.ScopedEditorTransaction('Replace range daylight with reusable environment'):
        environment = environments[0] if environments else actors.spawn_actor_from_class(unreal.OpenGolfEnvironment, unreal.Vector(0,0,0))
        environment.set_actor_label('Environment_Daylight')
        environment.set_folder_path('Range/Lighting')
        environment.set_editor_property('profile', profile)
        ranges[0].modify()
        for actor in legacy:
            actors.destroy_actor(actor)
        ranges[0].build_default_layout()
        environment.reset_to_course_defaults()
    if not level.save_current_level():
        raise RuntimeError('Migration save failed; backups are in ' + str(backup))
    unreal.log('OPEN_GOLF_ENVIRONMENT_MIGRATION_COMPLETE backup=' + str(backup))

if __name__ == '__main__':
    migrate()
