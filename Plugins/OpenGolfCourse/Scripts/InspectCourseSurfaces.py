# SPDX-License-Identifier: MIT
"""Print the open map's surface presets, their materials and placed grass, without changing or saving anything."""
from collections import Counter
import unreal as ue


def inspect():
    editor = ue.get_editor_subsystem(ue.UnrealEditorSubsystem)
    world = editor.get_editor_world()
    ue.log('COURSE_CHECK map=' + (world.get_path_name() if world else 'none'))
    ue.log('COURSE_CHECK play=' + str(bool(editor.get_game_world())))
    actors = ue.get_editor_subsystem(ue.EditorActorSubsystem).get_all_level_actors()
    surfaces = {}
    for actor in actors:
        if isinstance(actor, ue.OpenGolfTerrainArea) and actor.definition:
            for region in actor.definition.regions:
                if region.surface and region.surface.material:
                    surfaces[region.surface.material] = region.surface.surface
                    ue.log(f'COURSE_CHECK region={region.name} surface={region.surface.surface} '
                           f'preset={region.surface.get_path_name()} material={region.surface.material.get_path_name()}')
    grass = Counter()
    for actor in actors:
        for component in actor.get_components_by_class(ue.StaticMeshComponent):
            mesh = component.static_mesh
            if not mesh:
                continue
            for slot in range(component.get_num_materials()):
                material = component.get_material(slot)
                if material in surfaces:
                    ue.log(f'COURSE_CHECK surface_actor={actor.get_actor_label()} surface={surfaces[material]} '
                           f'assigned={material.get_path_name()}')
            if 'grass' in mesh.get_path_name().lower():
                count = component.get_instance_count() if isinstance(component, ue.InstancedStaticMeshComponent) else 1
                grass[mesh.get_path_name()] += count
    for mesh, count in sorted(grass.items()):
        ue.log(f'COURSE_CHECK grass={mesh} instances={count}')
    ue.log('COURSE_CHECK complete; no changes made')


if __name__ == '__main__':
    inspect()
