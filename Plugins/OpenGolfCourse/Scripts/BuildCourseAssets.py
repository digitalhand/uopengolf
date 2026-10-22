# SPDX-License-Identifier: MIT
"""Build or update every OpenGolfCourse asset under /OpenGolfCourse.

Run PrepareCourseAssets.py in Blender first (it exports the FBX meshes), then run this in the
editor's Output Log (Cmd: Python) with  py "<absolute path to this file>"  or headless with
UnrealEditor-Cmd <project> -ExecutePythonScript=<absolute path> (not the Python commandlet:
LOD generation needs editor subsystems). Rerunning updates assets in place. The PM_* physical
materials need the compiled OpenFairwayPhysics module; until then they are skipped with a warning.
"""
from pathlib import Path
import importlib
import sys

sys.path.insert(0, str(Path(__file__).resolve().parent))

import unreal as ue  # noqa: E402
import course_assets  # noqa: E402
import course_props  # noqa: E402
import course_surfaces  # noqa: E402

# A rerun in the same editor session must pick up edits to the builder modules.
for module in (course_assets, course_surfaces, course_props):
    importlib.reload(module)

course_surfaces.build_surfaces()
course_props.build_props()
ue.log("OPEN_GOLF_COURSE_ASSETS_COMPLETE")
