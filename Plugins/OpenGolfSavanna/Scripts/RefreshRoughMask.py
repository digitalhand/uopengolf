# SPDX-License-Identifier: MIT
"""Re-bake the savanna ground's rough mask from the course's current outlines.

    py "<project>/Plugins/OpenGolfSavanna/Scripts/RefreshRoughMask.py"

Open /Game/UOpenGolf/Maps/L_SavannaRange with Play stopped. The mask (T_SavannaRoughMask) makes the ground under the rough
tiles rough-coloured within 12 m of play and fades it to desert by 16 m. BuildSavannaRange.py bakes it; until 2026-11-23
BuildParFourHole.py did not, so the mask still described the range before the par-4 hole. The ground beside the back of
the green and the greenside bunker was desert, and it showed in steps between the square rough tiles and the curved
fringe.

The script backs up the texture and its source to Saved/TurfAssetBackups/<time>-before-rough-mask/, re-bakes the mask
in place with the same bounds and checks that every play outline is solid rough. Only the texture changes: the material
instances, terrain, grass and map are kept. OPEN_GOLF_ROUGH_MASK_REFRESHED confirms the end.
"""
from datetime import datetime, timezone
import importlib
from pathlib import Path
import shutil
import sys

import unreal as ue

HERE = Path(__file__).resolve().parent
PROJECT = HERE.parents[2]
sys.path.insert(0, str(HERE))
sys.path.insert(0, str(PROJECT / 'Plugins/OpenGolfCourse/Scripts'))
import ApplyCourseGrass as course
import ApplyCutTurf as apply_turf
import BuildSavannaRange as savanna
import savanna_layout as layout

course, apply_turf, savanna, layout = [importlib.reload(m) for m in (course, apply_turf, savanna, layout)]
MASK = f'{savanna.COURSE}/Materials/{savanna.ROUGH_MASK}'


def backup():
    folder = PROJECT / 'Saved/TurfAssetBackups' / datetime.now(timezone.utc).strftime('%Y%m%d-%H%M%S-before-rough-mask')
    sources = [course.package_file(MASK, '.uasset'), savanna.ground.mask_source(savanna.ROUGH_MASK)]
    missing = [str(path) for path in sources if not path.exists()]
    if missing:
        raise RuntimeError(f'Nothing to back up at {", ".join(missing)}; nothing was changed')
    folder.mkdir(parents=True, exist_ok=False)
    for path in sources:
        shutil.copy2(path, folder / path.name)
    return folder


def uncovered_before(definition):
    """Play outline points the current source leaves short of solid rough; the texture was imported from it."""
    minimum = definition.get_editor_property('detail_minimum_m')
    maximum = definition.get_editor_property('detail_maximum_m')
    return len(layout.mask_shortfall(savanna.ground.mask_values(savanna.ROUGH_MASK),
                                     savanna.ground.play_outlines(definition), (minimum.x, minimum.y),
                                     (maximum.x, maximum.y), *savanna.ground.MASK_SIZE))


def refresh():
    area = course.terrain_area(apply_turf.MAP)
    definition = area.definition
    savanna.check_mask_bounds(definition)
    before = uncovered_before(definition)
    folder = backup()
    checked = savanna.refresh_rough_mask(definition)
    ue.log(f'OPEN_GOLF_ROUGH_MASK_REFRESHED {MASK}: {checked} play outline points solid rough '
           f'(the previous mask left {before} short); backup={folder}')


if __name__ == '__main__':
    refresh()
