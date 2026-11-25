# SPDX-License-Identifier: MIT
"""Build a Fedora/openSUSE x86_64 package from an already cooked Linux game, without root.

rpmbuild only packages the staged files: nothing is compiled, stripped or rewritten, so Ubuntu's `rpm`
package can build it on the same runner as the .deb.
"""
import argparse
from pathlib import Path
import shlex
import subprocess
import sys

# Also allow direct execution from outside the repository.
sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from Assets.Installer.linux_payload import (  # noqa: E402
    APP, DESCRIPTION, HOMEPAGE, KEYRING, RELEASE_KEY, SUMMARY, UPDATE_INSTALLER, package_build, package_version,
    publish, stage_tree,
)

# Library names rather than package names: Fedora and openSUSE name their packages differently
# (vulkan-loader, libvulkan1) but both provide these. ELF dependencies plus SDL's dynamically loaded
# desktop/audio libraries, as in the .deb. In-game updates: pkexec asks for the password and gpgv checks
# the release's signature.
REQUIRES = (
    "libc.so.6()(64bit)", "libgcc_s.so.1()(64bit)", "libstdc++.so.6()(64bit)", "libz.so.1()(64bit)",
    "liblz4.so.1()(64bit)", "libzstd.so.1()(64bit)", "libvulkan.so.1()(64bit)", "libX11.so.6()(64bit)",
    "libXext.so.6()(64bit)", "libXi.so.6()(64bit)", "libXrandr.so.2()(64bit)", "libXcursor.so.1()(64bit)",
    "libXfixes.so.3()(64bit)", "libXss.so.1()(64bit)", "libwayland-client.so.0()(64bit)",
    "libwayland-cursor.so.0()(64bit)", "libwayland-egl.so.1()(64bit)", "libxkbcommon.so.0()(64bit)",
    "libasound.so.2()(64bit)", "libpulse.so.0()(64bit)", "libsecret-1.so.0()(64bit)", "libsystemd.so.0()(64bit)",
    "/usr/bin/pkexec", "/usr/bin/gpgv", "xdg-utils", "ca-certificates",
)
# Directories this package owns outright; every other staged file is listed by itself, so shared
# directories such as /usr/bin and the icon theme stay owned by the system.
OWNED_DIRS = (APP, UPDATE_INSTALLER.parent, KEYRING.parent, Path("usr/share/doc/uopengolf"))
# Package the staged files as they are, and keep this machine's name out of the package header.
SPEC_HEADER = """\
%global debug_package %{nil}
%global __strip /bin/true
%global __os_install_post %{nil}
%global __arch_install_post %{nil}
%global _build_id_links none
%global _binary_payload w1.gzdio
%global _buildhost uopengolf-build
"""


def rpm_version(version: str) -> str:
    # rpm forbids "-" in a version; ~ sorts a prerelease before its release, as in the .deb.
    return package_version(version).replace("-", "_")


def spec_path(path: Path) -> str:
    """A quoted %files entry; % is rpm's macro character."""
    text = "/" + path.as_posix()
    if '"' in text or "\n" in text:
        raise ValueError(f"Unsupported file name for rpm: {text}")
    return '"' + text.replace("%", "%%") + '"'


def files_list(stage: Path) -> list[str]:
    owned = [stage / directory for directory in OWNED_DIRS]
    entries = [spec_path(directory) for directory in OWNED_DIRS]
    for path in sorted(stage.rglob("*")):
        if path.is_dir() or any(path.is_relative_to(directory) for directory in owned):
            continue
        entries.append(spec_path(path.relative_to(stage)))
    return entries


def check_work_path(path: Path) -> None:
    """The work folder appears in the spec, where % starts an rpm macro."""
    if "%" in str(path) or "\n" in str(path):
        raise ValueError("The output directory's path can't contain % or a line break.")


def write_spec(stage: Path, spec: Path, version: str) -> None:
    requires = "".join(f"Requires: {name}\n" for name in REQUIRES)
    files = "\n".join(files_list(stage))
    # Quoted for the %install shell script, so a $ or ` in the path stays literal.
    root = shlex.quote(f"{stage}/.")
    spec.write_text(
        SPEC_HEADER
        + f"Name: uopengolf\nVersion: {rpm_version(version)}\nRelease: 1\nSummary: {SUMMARY}\n"
        "License: MIT AND LicenseRef-proprietary\n"
        f"URL: {HOMEPAGE}\nExclusiveArch: x86_64\nAutoReqProv: no\n{requires}\n"
        f"%description\n{DESCRIPTION}\n\n"
        # Hard links: the game is gigabytes, and the stage is on the same file system.
        f'%install\ncp -al {root} "%{{buildroot}}/" || cp -a {root} "%{{buildroot}}/"\n\n'
        f"%files\n%defattr(-,root,root,-)\n{files}\n",
        encoding="utf-8",
    )


def build_package(package_dir: Path, version: str, output_dir: Path, signing_key: Path = RELEASE_KEY) -> Path:
    rpm_version(version)
    name = f"UOpenGolf-{version}-Linux-x86_64.rpm"
    with package_build(package_dir, output_dir, "rpmbuild",
                       "Install rpm on the build machine (sudo apt install rpm); rpmbuild is required.",
                       ".uopengolf-rpm-") as (source, work):
        # Before staging, so a bad path fails before gigabytes are copied.
        check_work_path(work)
        stage = work / "root"
        stage_tree(source, stage, version, "rpm", signing_key)
        spec = work / "uopengolf.spec"
        write_spec(stage, spec, version)
        subprocess.run([
            "rpmbuild", "-bb", "--quiet", "--target", "x86_64",
            "--define", f"_topdir {work / 'rpmbuild'}",
            "--define", f"_rpmdir {work}",
            "--define", f"_rpmfilename {name}",
            str(spec),
        ], check=True)
        return publish(work / name, output_dir)


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--package-dir", required=True, type=Path)
    parser.add_argument("--version", required=True)
    parser.add_argument("--output-dir", required=True, type=Path)
    args = parser.parse_args(argv)
    try:
        print(build_package(args.package_dir, args.version, args.output_dir))
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        print(f"RPM package build failed: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
