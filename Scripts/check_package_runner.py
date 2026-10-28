# SPDX-License-Identifier: MIT
"""Check a self-hosted build machine and its public checkout before packaging."""
import argparse
import csv
import io
import json
import os
from pathlib import Path
import platform
import re
import shutil
import subprocess
import sys


ENGINE_VERSION = (5, 8, 4)
ENGINE_NAME = "Unreal Engine " + ".".join(map(str, ENGINE_VERSION))
PRIVATE_PATHS = (
    "Plugins/PrivateContent",
    "Content/PN_GrassLibrary",
    "Content/DZ_Assets",
    "Content/MWLandscapeAutoMaterial",
)


def run(command: list[str], cwd: Path | None = None) -> str:
    result = subprocess.run(
        command, cwd=cwd, capture_output=True, text=True, encoding="utf-8", errors="replace", timeout=30,
    )
    if result.returncode:
        raise ValueError(f"{Path(command[0]).name} failed. Check that the runner's tools are installed and usable.")
    return result.stdout


def require_files(paths: list[Path], instruction: str) -> None:
    for path in paths:
        if not path.is_file():
            raise ValueError(f"Missing {path}. {instruction}")


def check_engine(engine: Path, system: str) -> None:
    version_file = engine / "Engine/Build/Build.version"
    require_files([version_file], f"Install {ENGINE_NAME} and set UE_ROOT on the runner.")
    version = json.loads(version_file.read_text(encoding="utf-8-sig"))
    actual = tuple(version.get(key) for key in ("MajorVersion", "MinorVersion", "PatchVersion"))
    if actual != ENGINE_VERSION:
        raise ValueError(f"Unreal Engine {actual} does not match {ENGINE_NAME}. Set UE_ROOT to the correct installation.")
    tools = (
        ["Binaries/Linux/UnrealEditor", "Build/BatchFiles/Linux/Build.sh", "Build/BatchFiles/RunUAT.sh"]
        if system == "Linux" else
        ["Binaries/Win64/UnrealEditor.exe", "Binaries/Win64/UnrealEditor-Cmd.exe",
         "Build/BatchFiles/Build.bat", "Build/BatchFiles/RunUAT.bat",
         "Binaries/ThirdParty/Python3/Win64/python.exe"]
    )
    require_files([engine / "Engine" / tool for tool in tools], "Repair the Unreal Engine installation.")


def check_checkout(project: Path) -> None:
    for name in PRIVATE_PATHS:
        path = project / name
        if path.exists() or path.is_symlink():
            raise ValueError(f"Restricted content found at {name}. Use a separate, clean runner checkout.")
    if any((project / "Config").glob("User*.ini")):
        raise ValueError("Local Config/User*.ini files found. Use a separate, clean runner checkout.")
    assets = run(["git", "ls-files", "-z", "--", "*.uasset", "*.umap"], cwd=project).split("\0")
    if not any(assets):
        raise ValueError("No tracked Unreal assets found. Check out the full repository with Git LFS enabled.")
    for name in filter(None, assets):
        with (project / name).open("rb") as asset:
            if asset.read(128).startswith(b"version https://git-lfs.github.com/spec/v1"):
                raise ValueError(f"Git LFS pointer found at {name}. Run git lfs pull in the runner checkout.")


def check_no_editor(system: str) -> None:
    if system == "Windows":
        rows = csv.reader(io.StringIO(run(["tasklist.exe", "/FO", "CSV", "/NH"])))
        editors = [f"{row[0]} (PID {row[1]})" for row in rows
                   if len(row) > 1 and re.match(r"^UnrealEditor(?:$|[-.])", row[0], re.IGNORECASE)]
    else:
        rows = [line.split(maxsplit=1) for line in run(["ps", "-eo", "pid=,comm="]).splitlines()]
        editors = [f"{row[1]} (PID {row[0]})" for row in rows
                   if len(row) == 2 and re.match(r"^UnrealEditor(?:$|[-.])", row[1], re.IGNORECASE)]
    if editors:
        raise ValueError(f"Unreal editor already running: {', '.join(editors)}. Close it yourself before rerunning the job.")


def registry_path(key: str, value: str) -> Path | None:
    import winreg

    for view in (winreg.KEY_WOW64_32KEY, winreg.KEY_WOW64_64KEY):
        try:
            with winreg.OpenKey(winreg.HKEY_LOCAL_MACHINE, key, 0, winreg.KEY_READ | view) as handle:
                return Path(winreg.QueryValueEx(handle, value)[0])
        except FileNotFoundError:
            continue
    return None


def check_windows_tools() -> None:
    vswhere = Path(os.environ.get("ProgramFiles(x86)", r"C:\Program Files (x86)")) / "Microsoft Visual Studio/Installer/vswhere.exe"
    require_files([vswhere], "Install Visual Studio 2022 17.14 or later with Game development with C++.")
    installations = run([
        str(vswhere), "-utf8", "-products", "*", "-version", "[17.14,)",
        "-requires", "Microsoft.VisualStudio.Component.VC.Tools.x86.x64", "-property", "installationPath",
    ]).splitlines()
    if not any(list(Path(root).glob("VC/Tools/MSVC/*/bin/Hostx64/x64/cl.exe")) for root in installations):
        raise ValueError("MSVC x64 compiler not found. Install Game development with C++ in Visual Studio 2022 17.14 or later.")
    sdk = registry_path(r"SOFTWARE\Microsoft\Windows Kits\Installed Roots", "KitsRoot10")
    includes = list((sdk / "Include").glob("10.*")) if sdk else []
    if not any(all(path.is_file() for path in (
        include / "um/Windows.h", include / "ucrt/stdlib.h", include / "cppwinrt/winrt/base.h",
        sdk / "Lib" / include.name / "um/x64/kernel32.lib",
        sdk / "Lib" / include.name / "ucrt/x64/ucrt.lib",
        sdk / "bin" / include.name / "x64/rc.exe",
    )) for include in includes):
        raise ValueError("Windows SDK with x64 libraries and C++/WinRT headers not found. Install it through Visual Studio Installer.")
    netfx_versions = ("4.8.1", "4.8", "4.7.2", "4.7.1", "4.7", "4.6.2", "4.6.1", "4.6")
    netfx_roots = [registry_path(r"SOFTWARE\Microsoft\Microsoft SDKs\NETFXSDK" + "\\" + version,
                                 "KitsInstallationFolder") for version in netfx_versions]
    if not any(root and (root / "Include/um/mscoree.h").is_file() for root in netfx_roots):
        raise ValueError(".NET Framework SDK not found. Install the 4.8 SDK/developer tools through Visual Studio Installer.")
    require_files([Path(os.environ["SystemRoot"]) / "System32/tar.exe"], "Install Windows' tar archive tool.")


def check_tools(engine: Path, system: str) -> None:
    commands = ["git", "tasklist.exe", "powershell", "taskkill.exe"] if system == "Windows" else [
        "git", "python3", "bash", "timeout", "tar", "sha256sum", "find", "ps",
    ]
    for command in commands:
        if not shutil.which(command):
            raise ValueError(f"Missing {command}. Install it and add it to the runner's PATH.")
    try:
        run(["git", "lfs", "version"])
    except ValueError as error:
        raise ValueError("Git LFS is not usable. Install Git LFS on the runner and run git lfs install.") from error
    if system == "Windows":
        check_windows_tools()
    else:
        if not os.access(engine / "Engine/Binaries/Linux/UnrealEditor", os.X_OK):
            raise ValueError("UnrealEditor is not executable. Repair the engine's file permissions.")
        if "GNU tar" not in run(["tar", "--version"]):
            raise ValueError("GNU tar is required to create the Linux archives. Install it on the runner.")
        toolchain = os.environ.get("LINUX_MULTIARCH_ROOT")
        sdk = engine / "Engine/Extras/ThirdPartyNotUE/SDKs/HostLinux/Linux_x64"
        compilers = (
            [Path(toolchain) / "x86_64-unknown-linux-gnu/bin/clang++"] if toolchain else
            list(sdk.glob("v26_*/x86_64-unknown-linux-gnu/bin/clang++"))
        )
        if not any(path.is_file() and "20.1.8" in run([str(path), "--version"]) for path in compilers):
            raise ValueError("Unreal's Clang 20.1.8 toolchain is missing. Install the engine's v26 Linux toolchain.")


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--engine-root", default=os.environ.get("UE_ROOT"))
    args = parser.parse_args(argv)
    system = platform.system()
    if system not in ("Linux", "Windows") or platform.machine().lower() not in ("x86_64", "amd64"):
        print("Packaging requires a Linux or Windows x64 runner.", file=sys.stderr)
        return 1
    if not args.engine_root:
        print(f"Set UE_ROOT on the runner to its {ENGINE_NAME} installation.", file=sys.stderr)
        return 1
    engine = Path(args.engine_root)
    project = Path(__file__).resolve().parent.parent
    try:
        check_engine(engine, system)
        check_tools(engine, system)
        check_checkout(project)
        check_no_editor(system)
    except (OSError, ValueError, subprocess.SubprocessError) as error:
        print(f"Package runner check failed: {error}", file=sys.stderr)
        return 1
    print(f"Package runner ready: {system} x64, {ENGINE_NAME}.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
