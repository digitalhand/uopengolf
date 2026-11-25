# SPDX-License-Identifier: MIT
"""Stage a cooked Linux game as an installed system tree, shared by the .deb and .rpm builders."""
import base64
from collections.abc import Iterator
from contextlib import contextmanager
from datetime import date, datetime, timezone
import hashlib
import os
from pathlib import Path
import shutil
import sys
import tempfile
from xml.etree import ElementTree

# Also allow direct execution from outside the repository.
PROJECT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(PROJECT))
from Assets.Scripts.project_version import VERSION  # noqa: E402


INSTALLER = Path(__file__).resolve().parent
ICONS = PROJECT / "Assets/Images/Icons/png"
# Sizes installed into the hicolor theme for uopengolf.desktop's Icon=uopengolf.
ICON_SIZES = (16, 24, 32, 48, 64, 128, 256, 512)
# AppStream component ID: software centres read /usr/share/metainfo/<ID>.metainfo.xml.
METAINFO_ID = "io.github.digitalhand.uopengolf"
METAINFO_TOKENS = ("@VERSION@", "@DATE@", "@TYPE@")
BINARY = Path("UOpenGolf/Binaries/Linux/UOpenGolf")
APP = Path("opt/uopengolf")
LAUNCHER = Path("usr/bin/uopengolf")
# In-game updates: pkexec runs this root helper through the polkit action below.
UPDATE_INSTALLER = Path("usr/lib/uopengolf/install-update")
POLKIT_ACTION = Path("usr/share/polkit-1/actions/io.github.digitalhand.uopengolf.update.policy")
# install-update only installs packages listed in a SHA256SUMS that this key signed (see CodeSigning.md).
RELEASE_KEY = INSTALLER / "release-signing-key.asc"
KEYRING = Path("usr/share/uopengolf/release-signing-key.gpg")
# Tells the game which format installed it, so it downloads the same format (OpenGameUpdate's .package-format).
PACKAGE_FORMAT = APP / ".package-format"
PACKAGE_FORMATS = ("deb", "rpm")
SUMMARY = "Golf practice with launch monitor support"
DESCRIPTION = "Practice on the UOpenGolf range with a compatible Vulkan graphics device."
HOMEPAGE = "https://github.com/digitalhand/uopengolf"


def release_date() -> date:
    return datetime.now(timezone.utc).date()


def check_version(version: str) -> None:
    if not VERSION.fullmatch(version):
        raise ValueError("Version must be MAJOR.MINOR.PATCH[-prerelease].")


def package_version(version: str) -> str:
    """The version for dpkg and rpm: a prerelease sorts before its release with ~."""
    check_version(version)
    return version.replace("-", "~", 1)


def appstream_metadata(version: str) -> str:
    """Return the AppStream metainfo with this build's release filled in."""
    check_version(version)  # Validates the version before it goes into XML.
    text = (INSTALLER / "uopengolf.metainfo.xml").read_text(encoding="utf-8")
    values = (version, release_date().isoformat(), "development" if "-" in version else "stable")
    for token, value in zip(METAINFO_TOKENS, values):
        if token not in text:
            raise ValueError(f"uopengolf.metainfo.xml has no {token} placeholder.")
        text = text.replace(token, value)
    try:
        ElementTree.fromstring(text)
    except ElementTree.ParseError as error:
        raise ValueError(f"uopengolf.metainfo.xml is not valid XML: {error}") from error
    return text


def check_package_dir(source: Path) -> None:
    """Fail fast on anything that isn't a cooked Linux x86-64 game."""
    for path in (BINARY, Path("NOTICES.txt")):
        if not (source / path).is_file():
            raise ValueError(f"Missing packaged file: {source / path}")
    if not (source / "UOpenGolf/Content/Paks").is_dir() or not (source / "Engine").is_dir():
        raise ValueError("Package needs UOpenGolf/Content/Paks and Engine directories.")
    with (source / BINARY).open("rb") as executable:
        header = executable.read(20)
    if header[:6] != b"\x7fELF\x02\x01" or header[18:20] != b"\x3e\x00":
        raise ValueError("UOpenGolf must be a Linux x86-64 ELF executable.")


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


def copy_file(source: Path, destination: Path) -> None:
    destination.parent.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(source, destination)


def stage_desktop_files(stage: Path, metadata: str) -> None:
    """Add the launcher, menu entry, icons, AppStream metadata and copyright."""
    copy_file(INSTALLER / "uopengolf", stage / LAUNCHER)
    copy_file(INSTALLER / "uopengolf.desktop", stage / "usr/share/applications/uopengolf.desktop")
    for icon_size in ICON_SIZES:
        copy_file(ICONS / f"UOpenGolf_{icon_size}.png",
                  stage / f"usr/share/icons/hicolor/{icon_size}x{icon_size}/apps/uopengolf.png")
    metainfo = stage / f"usr/share/metainfo/{METAINFO_ID}.metainfo.xml"
    metainfo.parent.mkdir(parents=True)
    metainfo.write_text(metadata, encoding="utf-8")
    copy_file(PROJECT / "LICENSE", stage / "usr/share/doc/uopengolf/copyright")


def crc24(data: bytes) -> int:
    """The OpenPGP ASCII armor checksum (RFC 4880, section 6.1)."""
    crc = 0xB704CE
    for byte in data:
        crc ^= byte << 16
        for _ in range(8):
            crc <<= 1
            if crc & 0x1000000:
                crc ^= 0x1864CFB
    return crc & 0xFFFFFF


def dearmor_public_key(text: str) -> bytes:
    """The binary keyring gpgv reads, from an ASCII-armored public key, without needing gpg on the build machine."""
    lines = [line.strip() for line in text.splitlines()]
    try:
        start = lines.index("-----BEGIN PGP PUBLIC KEY BLOCK-----")
        end = lines.index("-----END PGP PUBLIC KEY BLOCK-----", start)
        # Armor headers, if any, end at the first blank line.
        body = lines[lines.index("", start) + 1:end]
    except ValueError as error:
        raise ValueError("The release key is not an ASCII-armored public key.") from error
    checksum = [line for line in body if line.startswith("=")]
    data = base64.b64decode("".join(line for line in body if line and not line.startswith("=")), validate=True)
    # A public key packet (tag 6) comes first, in either packet format.
    if not data or data[0] not in (0x99, 0x98, 0x9A, 0xC6):
        raise ValueError("The release key does not start with a public key packet.")
    if checksum and int.from_bytes(base64.b64decode(checksum[0][1:]), "big") != crc24(data):
        raise ValueError("The release key's armor checksum doesn't match.")
    return data


def stage_updater(stage: Path, package_format: str, signing_key: Path = RELEASE_KEY) -> None:
    """Add the in-game update's root helper, its polkit action, the release key and the format marker."""
    if package_format not in PACKAGE_FORMATS:
        raise ValueError(f"Unknown package format: {package_format}")
    copy_file(INSTALLER / "install-update", stage / UPDATE_INSTALLER)
    copy_file(INSTALLER / POLKIT_ACTION.name, stage / POLKIT_ACTION)
    keyring = stage / KEYRING
    keyring.parent.mkdir(parents=True, exist_ok=True)
    keyring.write_bytes(dearmor_public_key(signing_key.read_text(encoding="ascii")))
    marker = stage / PACKAGE_FORMAT
    marker.parent.mkdir(parents=True, exist_ok=True)
    marker.write_text(f"{package_format}\n", encoding="utf-8")


def stage_tree(source: Path, stage: Path, version: str, package_format: str,
               signing_key: Path = RELEASE_KEY) -> int:
    """Stage the whole installed tree under stage; return its size in KiB."""
    # Before copying the game, so a broken template or key fails fast.
    metadata = appstream_metadata(version)
    dearmor_public_key(signing_key.read_text(encoding="ascii"))
    copy_payload(source, stage / APP)
    stage_desktop_files(stage, metadata)
    stage_updater(stage, package_format, signing_key)
    executables = {stage / LAUNCHER, stage / APP / BINARY, stage / UPDATE_INSTALLER}
    size = 0
    for path in stage.rglob("*"):
        if path.is_dir():
            path.chmod(0o755)
        else:
            size += (path.stat().st_size + 1023) // 1024
            path.chmod(0o755 if path in executables or os.access(path, os.X_OK) else 0o644)
    stage.chmod(0o755)
    return size


@contextmanager
def package_build(package_dir: Path, output_dir: Path, tool: str, hint: str, prefix: str) -> Iterator[tuple[Path, Path]]:
    """Check the game and the build tool, then yield (source, work): the resolved game folder and a scratch
    folder inside output_dir, on the same file system so the finished package moves into place without copying."""
    source, output = package_dir.resolve(), output_dir.resolve()
    if output.is_relative_to(source):
        raise ValueError("Output directory must be outside the packaged game directory.")
    check_package_dir(source)
    if not shutil.which(tool):
        raise ValueError(hint)
    output.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix=prefix, dir=output) as temporary:
        yield source, Path(temporary)


def publish(built: Path, output_dir: Path) -> Path:
    """Write the package's .sha256, then move both into output_dir; return the package's new path."""
    checksum = write_checksum(built)
    result = output_dir.resolve() / built.name
    built.replace(result)
    checksum.replace(result.with_name(checksum.name))
    return result


def write_checksum(package: Path) -> Path:
    """Write <package>.sha256 in sha256sum format beside the package."""
    digest = hashlib.sha256()
    with package.open("rb") as archive:
        for chunk in iter(lambda: archive.read(1024 * 1024), b""):
            digest.update(chunk)
    checksum = package.with_name(f"{package.name}.sha256")
    checksum.write_text(f"{digest.hexdigest()}  {package.name}\n", encoding="utf-8")
    package.chmod(0o644)
    checksum.chmod(0o644)
    return checksum
