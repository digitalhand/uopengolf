# SPDX-License-Identifier: MIT
"""Build an Ubuntu amd64 package from an already cooked Linux game, without root."""
import argparse
from pathlib import Path
import subprocess
import sys

# Also allow direct execution from outside the repository.
sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from Assets.Installer.linux_payload import (  # noqa: E402,F401  (re-exported for callers and tests)
    DESCRIPTION, HOMEPAGE, ICON_SIZES, METAINFO_ID, RELEASE_KEY, SUMMARY, appstream_metadata, package_build,
    package_version, publish, release_date, stage_tree,
)

# ELF dependencies, plus SDL's dynamically loaded desktop/audio libraries.
# Ubuntu 24.04 uses the t64 ALSA package name. In-game updates: pkexec asks for the password and
# gpgv checks the release's signature.
DEPENDENCIES = (
    "libc6 (>= 2.35), libgcc-s1, libstdc++6 (>= 9), zlib1g, liblz4-1, libzstd1, "
    "libvulkan1, libx11-6, libxext6, libxi6, libxrandr2, libxcursor1, libxfixes3, "
    "libxss1, libwayland-client0, libwayland-cursor0, libwayland-egl1, libxkbcommon0, "
    "libasound2t64, libpulse0, libsecret-1-0, libsystemd0, xdg-utils, ca-certificates, pkexec, gpgv"
)


def debian_version(version: str) -> str:
    return package_version(version)


def build_package(package_dir: Path, version: str, output_dir: Path, signing_key: Path = RELEASE_KEY) -> Path:
    converted = debian_version(version)
    with package_build(package_dir, output_dir, "dpkg-deb",
                       "Install dpkg on the build machine; dpkg-deb is required.", ".uopengolf-deb-") as (source, work):
        stage = work / "root"
        size = stage_tree(source, stage, version, "deb", signing_key)
        control = stage / "DEBIAN"
        control.mkdir(mode=0o755)
        (control / "control").write_text(
            f"Package: uopengolf\nVersion: {converted}\nArchitecture: amd64\n"
            "Maintainer: UOpenGolf contributors <noreply@github.com>\n"
            "Section: games\nPriority: optional\n"
            f"Installed-Size: {size}\nDepends: {DEPENDENCIES}\n"
            f"Homepage: {HOMEPAGE}\n"
            f"Description: {SUMMARY}\n {DESCRIPTION}\n",
            encoding="utf-8",
        )
        (control / "control").chmod(0o644)
        built = work / f"UOpenGolf-{version}-Linux-amd64.deb"
        subprocess.run([
            "dpkg-deb", "--build", "--root-owner-group", "-Zgzip", "-z1", str(stage), str(built),
        ], check=True)
        return publish(built, output_dir)


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--package-dir", required=True, type=Path)
    parser.add_argument("--version", required=True)
    parser.add_argument("--output-dir", required=True, type=Path)
    args = parser.parse_args(argv)
    try:
        print(build_package(args.package_dir, args.version, args.output_dir))
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        print(f"Debian package build failed: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
