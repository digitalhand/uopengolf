# SPDX-License-Identifier: MIT
"""Smooth the public range's play outlines, then rebuild only its ground meshes and course grass.

    py "<project>/Plugins/OpenGolfSavanna/Scripts/SmoothCourseOutlines.py"

Open /Game/UOpenGolf/Maps/UOpenGolf with Play stopped. Each outline becomes a curve through its
authored corners (outline_smoothing.py), so the physical surfaces, the ground meshes and the turf
field follow the same curve. Trees, vegetation and Landscapes are kept. The map and definition are
backed up to Saved/TerrainBackups first. Afterwards bake the course field (see FixTurfTransitions.py).
Outlines that are already smooth are left as they are.
"""
from pathlib import Path
import sys

import unreal as ue

HERE = Path(__file__).resolve().parent
PROJECT = HERE.parents[2]
sys.path.insert(0, str(HERE))
sys.path.insert(0, str(PROJECT / 'Plugins/OpenGolfCourse/Scripts'))
import ApplyCourseGrass as course
import ApplyCutTurf as apply_turf
from outline_smoothing import crossing_outlines, smooth_outline

SURFACE_FOLDER = '/Game/UOpenGolf/Terrain/Generated'
# Sample spacing in metres. Fairway curves are broad; 1 m chords stay within a centimetre of the curve.
SPACING = {'FAIRWAY': 1.0}
DEFAULT_SPACING = 0.5


def smoothed_regions(definition):
    regions, changed = list(definition.regions), []
    for region in regions:
        surface = apply_turf.enum_name(region.surface.surface)
        spacing = SPACING.get(surface, DEFAULT_SPACING)
        points = [(p.x, p.y) for p in region.outline_m]
        longest = max(((a[0]-b[0])**2 + (a[1]-b[1])**2) ** .5 for a, b in zip(points, points[1:] + points[:1]))
        if longest <= spacing * 1.05:
            continue
        region.set_editor_property('outline_m', [ue.Vector2D(x, y) for x, y in smooth_outline(points, spacing)])
        region.set_editor_property('spacing_m', min(region.spacing_m, spacing))
        changed.append(f'{region.name} {len(points)}->{len(region.outline_m)}')
    crossings = crossing_outlines([[(p.x, p.y) for p in r.outline_m] for r in regions])
    if crossings:
        raise RuntimeError(f'Smoothed outlines would cross {crossings}; nothing was changed')
    return regions, changed


def smooth():
    area = course.terrain_area(apply_turf.MAP)
    definition = area.definition
    regions, changed = smoothed_regions(definition)
    if not changed:
        ue.log('OPEN_GOLF_OUTLINES_ALREADY_SMOOTH')
        return
    course.backup(apply_turf.MAP, definition)
    definition.modify()
    definition.set_editor_property('regions', regions)
    if ue.OpenGolfTerrainBuilder.rebuild_surface_meshes(area, SURFACE_FOLDER) is None:
        raise RuntimeError('Ground rebuild failed (see LogTemp); the outlines were not saved')
    if not ue.EditorAssetLibrary.save_loaded_asset(definition):
        raise RuntimeError('Could not save the terrain definition')
    apply_turf.write_course_spec(definition)
    grass = [p.get_path_name() for p in area.course_grass_presets]
    boundary = area.boundary_grass_preset.get_path_name() if area.boundary_grass_preset else None

    def finished():
        ue.log('OPEN_GOLF_OUTLINES_SMOOTHED ' + ', '.join(changed))

    # Course grass places against the new outlines and tee box; course.apply saves the map when PCG finishes,
    # then deletes the replaced ground build and grass runs.
    course.apply(apply_turf.MAP, apply_turf.GENERATED, grass, boundary, finished=finished,
                 cleanup=[apply_turf.GENERATED, SURFACE_FOLDER])


if __name__ == '__main__':
    smooth()
