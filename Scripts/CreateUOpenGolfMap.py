# SPDX-License-Identifier: MIT
"""Maintainer utility: generate the original blank startup map using Unreal Python.

Run with UnrealEditor-Cmd <project> -run=pythonscript -script=<this file>.
The generated .umap is committed; contributors do not need to run this script.
Refuse to overwrite an existing map.
"""
from pathlib import Path

import unreal

MAP = "/Game/UOpenGolf/Maps/UOpenGolf"
if (Path(unreal.Paths.project_content_dir()) / "UOpenGolf/Maps/UOpenGolf.umap").exists():
    raise RuntimeError(f"Refusing to overwrite existing map: {MAP}")

levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
if not levels.new_level(MAP):
    raise RuntimeError(f"Could not create {MAP}")
if not levels.save_current_level():
    raise RuntimeError(f"Could not save {MAP}")
unreal.log(f"Created original blank map: {MAP}")
