# SPDX-License-Identifier: MIT
"""Import the fetched Fluent Emoji avatars as UI textures in /OpenGolfSavanna/UI/Avatars.

    py "<project>/Plugins/OpenGolfSavanna/Scripts/ImportAvatars.py" [--art-root DIR] [--replace]

Run FetchFluentEmoji.py first: it pins and verifies the sources in <art root>/FluentEmoji. Each
avatar becomes T_Avatar_<Id>, drawn by Slate as is: the UI texture group, one mip, uncompressed
sRGB colour, never streamed. Existing textures are kept unless --replace is given; their settings
are checked either way. The game lists the avatars in Source/UOpenGolf/Private/Profile/AvatarCatalog.cpp.
Ends with OPEN_GOLF_AVATARS_IMPORTED in LogPython.
"""
from pathlib import Path
import importlib
import sys

import unreal as ue

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
sys.path.insert(0, str(HERE.parents[1] / "OpenGolfCourse/Scripts"))
import FetchFluentEmoji as fetch
from art_root import art_root

fetch = importlib.reload(fetch)
FOLDER = "/OpenGolfSavanna/UI/Avatars"
MARKER = "OPEN_GOLF_AVATARS_IMPORTED"
LIB = ue.EditorAssetLibrary


def asset_path(avatar):
    return f"{FOLDER}/T_Avatar_{avatar}"


def import_texture(source, avatar):
    task = ue.AssetImportTask()
    task.filename = str(source)
    task.destination_path = FOLDER
    task.destination_name = f"T_Avatar_{avatar}"
    task.automated = True
    task.replace_existing = True
    task.save = False
    # The texture factory imports synchronously under destination_name (Interchange would ignore the name).
    task.factory = ue.TextureFactory()
    ue.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    texture = LIB.load_asset(asset_path(avatar))
    if not texture:
        raise RuntimeError(f"Could not import {source}")
    return texture


def configure(texture):
    """Slate draws the avatar as is; returns whether a setting changed."""
    wanted = {
        "lod_group": ue.TextureGroup.TEXTUREGROUP_UI,
        "compression_settings": ue.TextureCompressionSettings.TC_EDITOR_ICON,
        "mip_gen_settings": ue.TextureMipGenSettings.TMGS_NO_MIPMAPS,
        "srgb": True,
        "never_stream": True,
    }
    changed = False
    for name, value in wanted.items():
        if texture.get_editor_property(name) != value:
            texture.set_editor_property(name, value)
            changed = True
    return changed


def main(argv):
    replace = "--replace" in argv
    sources = art_root(["--", *[arg for arg in argv if arg != "--replace"]]) / "FluentEmoji"
    imported, kept = 0, 0
    for avatar, _, digest, size in fetch.AVATARS:
        source = sources / f"{avatar}.png"
        if not fetch.matches(source, digest, size):
            raise RuntimeError(f"{source} is missing or does not match its pin; run FetchFluentEmoji.py first")
        path = asset_path(avatar)
        fresh = replace or not LIB.does_asset_exist(path)
        texture = import_texture(source, avatar) if fresh else LIB.load_asset(path)
        imported += fresh
        kept += not fresh
        if configure(texture) or fresh:
            LIB.save_loaded_asset(texture, only_if_is_dirty=False)
    ue.log(f"{MARKER} imported={imported} kept={kept} folder={FOLDER}")


main(sys.argv[1:])
