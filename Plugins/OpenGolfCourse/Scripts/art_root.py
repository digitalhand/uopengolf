# SPDX-License-Identifier: MIT
"""Where editable Blender projects live.

Blender projects and full-resolution sources stay outside the repository; only Unreal-ready
assets are committed. Scripts resolve the folder from `--art-root PATH` (after Blender's `--`),
then the UOPENGOLF_BLENDER_ROOT environment variable, then ~/Code/3D/blender/uopengolf.
"""
import os
import sys
from pathlib import Path

ENVIRONMENT = "UOPENGOLF_BLENDER_ROOT"
DEFAULT = Path("~/Code/3D/blender/uopengolf")


def script_arguments(argv):
    """The arguments Blender passes through to a script: everything after "--"."""
    return list(argv[argv.index("--") + 1:]) if "--" in argv else []


def art_root(argv=None, environ=None):
    args = script_arguments(sys.argv if argv is None else argv)
    if "--art-root" in args:
        index = args.index("--art-root") + 1
        if index >= len(args):
            raise ValueError("--art-root needs a folder")
        return Path(args[index]).expanduser()
    value = (os.environ if environ is None else environ).get(ENVIRONMENT)
    return Path(value).expanduser() if value else DEFAULT.expanduser()
