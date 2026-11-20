# SPDX-License-Identifier: MIT
"""Build an Ubuntu amd64 package from an already cooked Linux game, without root."""
import argparse
import hashlib
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

# Also allow direct execution from outside the repository.
PROJECT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(PROJECT))
from Assets.Scripts.project_version import VERSION  # noqa: E402


INSTALLER = Path(__file__).resolve().parent
ICONS = PROJECT / "Assets/Images/Icons/png"
# Sizes installed into the hicolor theme for uopengolf.desktop's Icon=uopengolf.
ICON_SIZES = (16, 24, 32, 48, 64, 128, 256, 512)
BINARY = Path("UOpenGolf/Binaries/Linux/UOpenGolf")
# ELF dependencies, plus SDL's dynamically loaded desktop/audio libraries.
# Ubuntu 24.04 uses the t64 ALSA package name.
DEPENDENCIES = (
    "libc6 (>= 2.35), libgcc-s1, libstdc++6 (>= 9), zlib1g, liblz4-1, libzstd1, "
    "libvulkan1, libx11-6, libxext6, libxi6, libxrandr2, libxcursor1, libxfixes3, "
    "libxss1, libwayland-client0, libwayland-cursor0, libwayland-egl1, libxkbcommon0, "
    "libasound2t64, libpulse0, libsecret-1-0, libsystemd0, xdg-utils, ca-certificates"
)


def debian_version(version: str) -> str:
    if not VERSION.fullmatch(version):
        raise ValueError("Version must be MAJOR.MINOR.PATCH[-prerelease].")
    return version.replace("-", "~", 1)


def copy_payload(source: Path, destination: Path) -> None:
    def excluded(directory: str, names: list[str]) -> set[str]:
        return {name for name in names if name == "Saved" or name.endswith(".debug")
                or name == "Manifest_DebugFiles_Linux.txt"}

    # A staged game must not pull files from elsewhere on the build machine.
    for path in source.rglob("*"):
        if path.is_symlink() and not path.resolve().is_relative_to(source):
            raise ValueError(f"Package contains an external link: {path}")
    shutil.copytree(source, destination, ignore=excluded)
    for name in ("LICENSE", "THIRD_PARTY_NOTICES.md"):
        shutil.copyfile(PROJECT / name, destination / name)
    # The engine-generated launcher chmods the binary at every start. The installed
    # game is root-owned, so use the package launcher instead.
    (destination / "UOpenGolf.sh").unlink(missing_ok=True)


def build_package(package_dir: Path, version: str, output_dir: Path) -> Path:
    converted = debian_version(version)
    source, output = package_dir.resolve(), output_dir.resolve()
    if output.is_relative_to(source):
        raise ValueError("Output directory must be outside the packaged game directory.")
    for path in (BINARY, Path("NOTICES.txt")):
        if not (source / path).is_file():
            raise ValueError(f"Missing packaged file: {source / path}")
    if not (source / "UOpenGolf/Content/Paks").is_dir() or not (source / "Engine").is_dir():
        raise ValueError("Package needs UOpenGolf/Content/Paks and Engine directories.")
    with (source / BINARY).open("rb") as executable:
        header = executable.read(20)
    if header[:6] != b"\x7fELF\x02\x01" or header[18:20] != b"\x3e\x00":
        raise ValueError("UOpenGolf must be a Linux x86-64 ELF executable.")
    if not shutil.which("dpkg-deb"):
        raise ValueError("Install dpkg on the build machine; dpkg-deb is required.")

    output.mkdir(parents=True, exist_ok=True)
    name = f"UOpenGolf-{version}-Linux-amd64.deb"
    with tempfile.TemporaryDirectory(prefix=".uopengolf-deb-", dir=output) as temporary:
        stage = Path(temporary) / "root"
        app = stage / "opt/uopengolf"
        copy_payload(source, app)
        launcher = stage / "usr/bin/uopengolf"
        launcher.parent.mkdir(parents=True)
        shutil.copyfile(INSTALLER / "uopengolf", launcher)
        desktop = stage / "usr/share/applications/uopengolf.desktop"
        desktop.parent.mkdir(parents=True)
        shutil.copyfile(INSTALLER / "uopengolf.desktop", desktop)
        for icon_size in ICON_SIZES:
            icon = stage / f"usr/share/icons/hicolor/{icon_size}x{icon_size}/apps/uopengolf.png"
            icon.parent.mkdir(parents=True)
            shutil.copyfile(ICONS / f"UOpenGolf_{icon_size}.png", icon)
        docs = stage / "usr/share/doc/uopengolf"
        docs.mkdir(parents=True)
        shutil.copyfile(PROJECT / "LICENSE", docs / "copyright")

        size = 0
        for path in stage.rglob("*"):
            if path.is_dir():
                path.chmod(0o755)
            else:
                size += (path.stat().st_size + 1023) // 1024
                executable = path == launcher or path == app / BINARY or os.access(path, os.X_OK)
                path.chmod(0o755 if executable else 0o644)
        stage.chmod(0o755)
        control = stage / "DEBIAN"
        control.mkdir(mode=0o755)
        (control / "control").write_text(
            f"Package: uopengolf\nVersion: {converted}\nArchitecture: amd64\n"
            "Maintainer: UOpenGolf contributors <noreply@github.com>\n"
            "Section: games\nPriority: optional\n"
            f"Installed-Size: {size}\nDepends: {DEPENDENCIES}\n"
            "Homepage: https://github.com/digitalhand/uopengolf\n"
            "Description: Golf practice with launch monitor support\n"
            " Practice on the UOpenGolf range with a compatible Vulkan graphics device.\n",
            encoding="utf-8",
        )
        (control / "control").chmod(0o644)
        built = Path(temporary) / name
        subprocess.run([
            "dpkg-deb", "--build", "--root-owner-group", "-Zgzip", "-z1", str(stage), str(built),
        ], check=True)
        digest = hashlib.sha256()
        with built.open("rb") as archive:
            for chunk in iter(lambda: archive.read(1024 * 1024), b""):
                digest.update(chunk)
        checksum = Path(temporary) / f"{name}.sha256"
        checksum.write_text(f"{digest.hexdigest()}  {name}\n", encoding="utf-8")
        built.chmod(0o644)
        checksum.chmod(0o644)
        result = output / name
        built.replace(result)
        checksum.replace(output / checksum.name)
    return result


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
