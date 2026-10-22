#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Download pinned Poly Haven (CC0) sources into the Blender art root.

    python3 Plugins/OpenGolfSavanna/Scripts/FetchPolyHaven.py [asset ...] [--art-root DIR]

Each file lands in <art root>/PolyHaven/<asset>/ and must match the size and MD5 that Poly
Haven publishes for it; files that already match are kept. A SHA256SUMS file is written
beside them for provenance. Nothing here is committed: the export scripts turn these sources
into Unreal-ready assets. Poly Haven assets are CC0 (https://polyhaven.com/license).
"""
from pathlib import Path
import hashlib
import sys
import urllib.request

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "OpenGolfCourse/Scripts"))
from art_root import art_root

USER_AGENT = "UOpenGolf-FetchPolyHaven/1 (+https://github.com/digitalhand/uopengolf)"
_MODELS = "https://dl.polyhaven.org/file/ph-assets/Models/"
_TEXTURES = "https://dl.polyhaven.org/file/ph-assets/Textures/"
_JACARANDA = _MODELS + "png/2k/jacaranda_tree/jacaranda_tree_"
# (path inside the asset folder, url, md5, size), from https://api.polyhaven.com/files/<asset>.
ASSETS = {
    "jacaranda_tree": [
        # The .blend keeps Poly Haven's LODs and geometry-node leaves; the FBX realises 3.9M triangles.
        ("jacaranda_tree_2k.blend", "https://dl.polyhaven.org/file/ph-assets/Models/blend/2k/jacaranda_tree/jacaranda_tree_2k.blend",
         "f2d34acb2029056c1565c9af8f5da605", 180227903),
        ("jacaranda_tree_2k.fbx", "https://dl.polyhaven.org/file/ph-assets/Models/fbx/2k/jacaranda_tree/jacaranda_tree_2k.fbx",
         "9a51fd6cb3881c3486b9b1499d3e036e", 132437628),
        ("textures/jacaranda_tree_branches_diff_2k.png", _JACARANDA + "branches_diff_2k.png", "53819fe367b7ebc67cc51fbf2a235a69", 19551560),
        ("textures/jacaranda_tree_branches_nor_gl_2k.png", _JACARANDA + "branches_nor_gl_2k.png", "98af2803beca2055fde16a4ca8f2151a", 20594936),
        ("textures/jacaranda_tree_branches_rough_2k.png", _JACARANDA + "branches_rough_2k.png", "f2c70d75834aa97f7ef173c119f92f8d", 4730461),
        ("textures/jacaranda_tree_leaves_alpha_2k.png", _JACARANDA + "leaves_alpha_2k.png", "9f8afd60299fd936ef95a4c3235102cf", 1252539),
        ("textures/jacaranda_tree_leaves_diff_2k.png", _JACARANDA + "leaves_diff_2k.png", "b1afbff2fe1f085547dedd19eff1c842", 8173693),
        ("textures/jacaranda_tree_leaves_nor_gl_2k.png", _JACARANDA + "leaves_nor_gl_2k.png", "0774f9d98b568b50371f00c69c5e0a7d", 14968102),
        ("textures/jacaranda_tree_leaves_rough_2k.png", _JACARANDA + "leaves_rough_2k.png", "724d23832f99eeb6b2447f0a404e28aa", 3221437),
        ("textures/jacaranda_tree_trunk_diff_2k.png", _JACARANDA + "trunk_diff_2k.png", "54c203d836580c841bb5ac2e5cc6caba", 20442412),
        ("textures/jacaranda_tree_trunk_nor_gl_2k.png", _JACARANDA + "trunk_nor_gl_2k.png", "d594c65dc554b88081689346d00ea751", 20503957),
        ("textures/jacaranda_tree_trunk_rough_2k.png", _JACARANDA + "trunk_rough_2k.png", "df7d8846780826e8686f2b1b7a36f0c9", 6375484),
    ],
    # Rough grass. Each .blend is a geometry-node tuft set; its "include" textures (jpg/exr) are the
    # ones the project opens with, and the PNG maps are the sources ExportPolyHavenGrass.py exports.
    # Ground under the grass tiles: a dense leafy turf seen from above, so gaps between blades read as
    # grass rather than bare ground.
    "leafy_grass": [
        ("maps/leafy_grass_diff_2k.png", _TEXTURES + "png/2k/leafy_grass/leafy_grass_diff_2k.png",
         "91c82271318a5bbe69182404c6dff730", 23606854),
        ("maps/leafy_grass_nor_gl_2k.png", _TEXTURES + "png/2k/leafy_grass/leafy_grass_nor_gl_2k.png",
         "99b68312712462595976eb2baef58c6b", 24430051),
        ("maps/leafy_grass_rough_1k.png", _TEXTURES + "png/1k/leafy_grass/leafy_grass_rough_1k.png",
         "5dff12e1e81532a92d074e63473ab127", 2444449),
    ],
    # Desert sand, replacing the restricted MW sand (licence roadmap row 3).
    # 8-bit JPGs: Unreal imports Poly Haven's 16-bit PNGs as linear data and washes the colour out.
    "red_sand": [
        ("maps/red_sand_diff_2k.jpg", _TEXTURES + "jpg/2k/red_sand/red_sand_diff_2k.jpg",
         "2b3c5fd8aaa0b1cc354ece7f7068cd72", 2284307),
        ("maps/red_sand_nor_gl_2k.jpg", _TEXTURES + "jpg/2k/red_sand/red_sand_nor_gl_2k.jpg",
         "9cfede9a012e8530c0034ba0d16543d1", 4550804),
        ("maps/red_sand_rough_2k.jpg", _TEXTURES + "jpg/2k/red_sand/red_sand_rough_2k.jpg",
         "054f39db62d74feaa767c86c707b61ae", 2271870),
    ],
    "grass_medium_01": [
        ("grass_medium_01_2k.blend", _MODELS + "blend/2k/grass_medium_01/grass_medium_01_2k.blend",
         "f301cca3574d007b7b67c68476936c1c", 1832480),
        ("textures/grass_medium_01_alpha_2k.png", _MODELS + "png/2k/grass_medium_01/grass_medium_01_alpha_2k.png",
         "0810166131307c1094d37e73a8490542", 1580653),
        ("textures/grass_medium_01_diff_2k.jpg", _MODELS + "jpg/2k/grass_medium_01/grass_medium_01_diff_2k.jpg",
         "000814eb2149add70695cf71bfb52c4b", 1166527),
        ("textures/grass_medium_01_dry_diff_2k.png", _MODELS + "png/2k/grass_medium_01/grass_medium_01_dry_diff_2k.png",
         "8ea1c1853688113b9e1e0b17c4efda5a", 6652964),
        ("textures/grass_medium_01_nor_gl_2k.exr", _MODELS + "exr/2k/grass_medium_01/grass_medium_01_nor_gl_2k.exr",
         "4a1e50ca39ce25a4b33f9086f578a632", 7389519),
        ("textures/grass_medium_01_rough_2k.exr", _MODELS + "exr/2k/grass_medium_01/grass_medium_01_rough_2k.exr",
         "8133a6d2d2fd01f899f12c27b7d854b7", 2627410),
        ("maps/grass_medium_01_diff_2k.png", _MODELS + "png/2k/grass_medium_01/grass_medium_01_diff_2k.png",
         "7a12aa81dfb2fb9527ef2456a50bfe11", 7090951),
        ("maps/grass_medium_01_nor_gl_2k.png", _MODELS + "png/2k/grass_medium_01/grass_medium_01_nor_gl_2k.png",
         "758babf47978df21207ea6c8fee893fe", 13080458),
        ("maps/grass_medium_01_rough_1k.png", _MODELS + "png/1k/grass_medium_01/grass_medium_01_rough_1k.png",
         "5aaa05d7168fe45514d5de1de0f0b472", 1157215),
        ("maps/grass_medium_01_ao_1k.png", _MODELS + "png/1k/grass_medium_01/grass_medium_01_ao_1k.png",
         "e34a168d3eeb006e31672306af432e8e", 642376),
    ],
}


def _digest(path, algorithm):
    hasher = hashlib.new(algorithm)
    with open(path, "rb") as handle:
        for block in iter(lambda: handle.read(1 << 20), b""):
            hasher.update(block)
    return hasher.hexdigest()


def matches(path, md5, size):
    path = Path(path)
    return path.is_file() and path.stat().st_size == size and _digest(path, "md5") == md5


def sha256sums(folder, paths):
    return "".join(f"{_digest(Path(folder) / path, 'sha256')}  {path}\n" for path in paths)


def download(url, target, md5, size):
    partial = target.with_name(target.name + ".part")
    request = urllib.request.Request(url, headers={"User-Agent": USER_AGENT})
    with urllib.request.urlopen(request, timeout=120) as response, open(partial, "wb") as handle:
        for block in iter(lambda: response.read(1 << 20), b""):
            handle.write(block)
    if not matches(partial, md5, size):
        partial.unlink()
        raise RuntimeError(f"{url} does not match its pinned size and MD5")
    partial.replace(target)


def fetch(name, root):
    folder = root / "PolyHaven" / name
    for path, url, md5, size in ASSETS[name]:
        target = folder / path
        target.parent.mkdir(parents=True, exist_ok=True)
        if matches(target, md5, size):
            print("Kept", target)
            continue
        print("Downloading", url)
        download(url, target, md5, size)
    (folder / "SHA256SUMS").write_text(sha256sums(folder, [path for path, *_ in ASSETS[name]]))
    print("POLYHAVEN_FETCHED", name, folder)
    return folder


def main(argv):
    names = [arg for index, arg in enumerate(argv) if not arg.startswith("--") and (index == 0 or argv[index - 1] != "--art-root")]
    unknown = sorted(set(names) - set(ASSETS))
    if unknown:
        raise SystemExit(f"Unknown assets: {', '.join(unknown)}; known: {', '.join(ASSETS)}")
    root = art_root(["--", *argv])
    for name in names or ASSETS:
        fetch(name, root)


if __name__ == "__main__":
    main(sys.argv[1:])
