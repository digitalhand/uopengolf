#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Download the pinned Fluent Emoji 3D images used as built-in player avatars.

    python3 Plugins/OpenGolfSavanna/Scripts/FetchFluentEmoji.py [--art-root DIR]

Each image lands in <art root>/FluentEmoji/<Id>.png and must match its pinned size and SHA-256;
files that already match are kept. The upstream LICENSE is saved beside them and a SHA256SUMS
file is written for provenance. Nothing here is committed: ImportAvatars.py turns these sources
into the Unreal textures under /OpenGolfSavanna/UI/Avatars. Fluent Emoji is MIT licensed
(Copyright (c) Microsoft Corporation); see Plugins/OpenGolfSavanna/NOTICE.md.
"""
from pathlib import Path
import hashlib
import struct
import sys
import urllib.parse
import urllib.request

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "OpenGolfCourse/Scripts"))
from art_root import art_root

USER_AGENT = "UOpenGolf-FetchFluentEmoji/1 (+https://github.com/digitalhand/uopengolf)"
REPOSITORY = "microsoft/fluentui-emoji"
# main on 2026-08-24, "Pin GitHub Actions to full-length commit SHAs (#193)".
COMMIT = "1ffb34c752ecf5d402f04cfb4b392c77f57c54bc"
SIDE = 256
LICENSE = ("LICENSE", "c2cfccb812fe482101a8f04597dfc5a9991a6b2748266c47ac91b6a5aae15383")
# (avatar id, path under assets/, sha256, size). Ids match AvatarCatalog.cpp and name T_Avatar_<Id>.
AVATARS = [
    ("Golfer", "Person golfing/Default/3D/person_golfing_3d_default.png",
     "05b970a20b6b4b1b116cb596ad660cb055f5c5ae43588f729d8edb0c9febf7bc", 25511),
    ("Flag", "Flag in hole/3D/flag_in_hole_3d.png",
     "92a644f41d5f4c1f75d774724028d0ffab82bc28b92d9ddf83fd4e7b0e405069", 28802),
    ("Trophy", "Trophy/3D/trophy_3d.png",
     "7c4a9805f354eb2aec35063a95a4043306efc726324c0d128883f24497857bbc", 31910),
    ("Birdie", "Bird/3D/bird_3d.png",
     "eeb6e0a3a54f7b5ec2b32995a1062ba3380eebd5f9a00d7158f3cfbb978c31a2", 32043),
    ("Eagle", "Eagle/3D/eagle_3d.png",
     "4ba56f4a18d6e9425809b67eaccd4133baca7bf388f0f36b4d3d6a301960c2d1", 24589),
    ("Bogey", "Ghost/3D/ghost_3d.png",
     "2a07c00a4af13532717a201e201b7219f02ce91232ea86abd7e74cbf1b5fb42f", 33215),
    ("DuckHook", "Duck/3D/duck_3d.png",
     "c45934c5d609fc03e9713d66ada5e08034b0b27bf29a37a39c726633b5c2d2e4", 32162),
    ("Goat", "Goat/3D/goat_3d.png",
     "eaefcf17760065c1c208018e06c0860f920b045cd418538330a5f80d51b0940a", 31083),
    ("SlowPlay", "Sloth/3D/sloth_3d.png",
     "000f1e461dfec9d43991b424b3d0ea4ccb6c342b7f8a1f507f716a460c662d64", 38149),
    ("Flamingo", "Flamingo/3D/flamingo_3d.png",
     "cda1374b5f4ad9aee904ce92af77b0279ffdf1aa438cb6123540811fa9d8781e", 23943),
    ("WaterHazard", "Crocodile/3D/crocodile_3d.png",
     "defa9f80af9049f9dc3e5f9732712c0f0bf6d72a8dcece2901a28aa4186bc75c", 39658),
    ("Giraffe", "Giraffe/3D/giraffe_3d.png",
     "bfe23a59e8b7f25b691403763f972f3da030edf199cb0665b21e01d1d1f6079c", 30236),
    ("Zebra", "Zebra/3D/zebra_3d.png",
     "0048ee247960f872952293d6baf6fcc833de4b126db2131d8df9d1f07f1da619", 38543),
    ("Hippo", "Hippopotamus/3D/hippopotamus_3d.png",
     "207bed240fcc3ed1f47ca0a65d3978be75e29c8e10dcd470d70cc2a95eb4d15c", 31035),
    ("Elephant", "Elephant/3D/elephant_3d.png",
     "d3046f53f45dc20b818484dec97f53634d5c71b277015fc96aebcdc0d49a8f91", 35170),
    ("Lion", "Lion/3D/lion_3d.png",
     "1f1cf0d0dd94915f3c9f10c3415c5efbd4b8725adfa44a5d564098547076b34b", 50059),
    ("Rhino", "Rhinoceros/3D/rhinoceros_3d.png",
     "4294c81d62ab0cc7822f8b98cc0249b384dd08071750907daf7e77b5cc32118a", 28183),
    ("GreenReader", "Owl/3D/owl_3d.png",
     "7dfcdcdebd599787719d98f3057b0822522678e6752369ca3498a780ca7ba443", 38126),
    ("Shades", "Smiling face with sunglasses/3D/smiling_face_with_sunglasses_3d.png",
     "e98cb43a7a531289c82eb57441eb20cd88cb53b895c212d8e3abf05dd2647da1", 47859),
    ("Cowboy", "Cowboy hat face/3D/cowboy_hat_face_3d.png",
     "9ef48091f71a81a04fe648cac438be0b0ae3f832879a9dd3451e0aaeb2b4732b", 46721),
    ("Incognito", "Disguised face/3D/disguised_face_3d.png",
     "0a0848bf71468635f752d46be66d8c292b400ec6502f5b8f9e1898d55c41ae90", 49564),
    ("SwingRobot", "Robot/3D/robot_3d.png",
     "3f9b5a00b53c8bddda36b6a3f68d00718d9d7d8c04f986a3248e55ae93a7961b", 43362),
    ("LongHitter", "Alien/3D/alien_3d.png",
     "7b2318a60f36814c289e92643cbfa8d72caac26d98e955d8cae196d3db9abddf", 38905),
    ("HoleInOne", "Unicorn/3D/unicorn_3d.png",
     "7d4a8a3e1040e9ba2b5a562413e258ebacc812e5c6d87946b3de53a66d1736b4", 35413),
]
PNG_SIGNATURE = b"\x89PNG\r\n\x1a\n"


def url(path):
    """The raw file at the pinned commit; path is relative to the repository root."""
    return f"https://raw.githubusercontent.com/{REPOSITORY}/{COMMIT}/{urllib.parse.quote(path)}"


def sha256(path):
    hasher = hashlib.sha256()
    with open(path, "rb") as handle:
        for block in iter(lambda: handle.read(1 << 20), b""):
            hasher.update(block)
    return hasher.hexdigest()


def matches(path, digest, size=None):
    path = Path(path)
    return path.is_file() and (size is None or path.stat().st_size == size) and sha256(path) == digest


def png_size(path):
    """(width, height) from a PNG's header, or None when the file is not a PNG."""
    with open(path, "rb") as handle:
        header = handle.read(24)
    if len(header) < 24 or header[:8] != PNG_SIGNATURE or header[12:16] != b"IHDR":
        return None
    return struct.unpack(">II", header[16:24])


def download(source, target, digest, size=None):
    partial = target.with_name(target.name + ".part")
    request = urllib.request.Request(source, headers={"User-Agent": USER_AGENT})
    with urllib.request.urlopen(request, timeout=60) as response, open(partial, "wb") as handle:
        for block in iter(lambda: response.read(1 << 16), b""):
            handle.write(block)
    if not matches(partial, digest, size):
        partial.unlink()
        raise RuntimeError(f"{source} does not match its pinned size and SHA-256")
    partial.replace(target)


def fetch(root):
    folder = Path(root) / "FluentEmoji"
    folder.mkdir(parents=True, exist_ok=True)
    names = []
    for avatar, path, digest, size in AVATARS:
        target = folder / f"{avatar}.png"
        if matches(target, digest, size):
            print("Kept", target)
        else:
            print("Downloading", avatar)
            download(url(f"assets/{path}"), target, digest, size)
        if png_size(target) != (SIDE, SIDE):
            raise RuntimeError(f"{target} is not a {SIDE} px square PNG")
        names.append(target.name)
    licence, digest = LICENSE
    if not matches(folder / licence, digest):
        download(url(licence), folder / licence, digest)
    names.append(licence)
    (folder / "SHA256SUMS").write_text("".join(f"{sha256(folder / name)}  {name}\n" for name in names))
    print("FLUENT_EMOJI_FETCHED", len(AVATARS), folder)
    return folder


if __name__ == "__main__":
    fetch(art_root(["--", *sys.argv[1:]]))
