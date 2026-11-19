# SPDX-License-Identifier: MIT
"""Where the shot library and the local reference data live.

The repository keeps only uopengolf's own shot recordings (Assets/Data/ShotLibrary). Third-party
reference data, accuracy baselines and papers stay outside it, in the folder named by the
UOPENGOLF_DATA environment variable, by default ~/data/uopengolf. See Assets/Data/README.md.
"""
import os
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent.parent
REPO_LIBRARY_DIR = ROOT / "Assets" / "Data" / "ShotLibrary"


def data_root():
    """The local reference data folder; it may not exist."""
    configured = os.environ.get("UOPENGOLF_DATA", "").strip()
    return Path(configured).expanduser() if configured else Path.home() / "data" / "uopengolf"


def local_library_dir():
    return data_root() / "shot_library"


def shot_library_dirs():
    """The repository's recordings, then the local reference sessions when that folder exists."""
    dirs = [REPO_LIBRARY_DIR]
    if local_library_dir().is_dir():
        dirs.append(local_library_dir())
    return dirs
