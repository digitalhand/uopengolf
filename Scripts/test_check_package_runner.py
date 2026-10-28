# SPDX-License-Identifier: MIT
"""Engine-free checks for the conditions that must stop a package job."""
import io
import json
import os
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest.mock import patch

from check_package_runner import (
    PRIVATE_PATHS,
    check_checkout,
    check_engine,
    check_no_editor,
    check_tools,
    check_windows_tools,
    main,
    run,
)


class PackageRunnerTests(unittest.TestCase):
    def setUp(self):
        directory = tempfile.TemporaryDirectory()
        self.addCleanup(directory.cleanup)
        self.root = Path(directory.name)
        self.engine = self.root / "engine"

    def write(self, relative, content="", root=None):
        path = (root or self.root) / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(content, encoding="utf-8")
        return path

    def make_engine(self, system="Linux", patch_version=3):
        self.write("Engine/Build/Build.version", json.dumps({
            "MajorVersion": 5, "MinorVersion": 8, "PatchVersion": patch_version,
        }), root=self.engine)
        tools = (
            ["Binaries/Linux/UnrealEditor", "Build/BatchFiles/Linux/Build.sh", "Build/BatchFiles/RunUAT.sh"]
            if system == "Linux" else
            ["Binaries/Win64/UnrealEditor.exe", "Binaries/Win64/UnrealEditor-Cmd.exe",
             "Build/BatchFiles/Build.bat", "Build/BatchFiles/RunUAT.bat",
             "Binaries/ThirdParty/Python3/Win64/python.exe"]
        )
        for tool in tools:
            self.write(f"Engine/{tool}", root=self.engine)

    def test_accepts_complete_engine_on_each_platform(self):
        for system in ("Linux", "Windows"):
            with self.subTest(system=system):
                self.make_engine(system)
                check_engine(self.engine, system)

    def test_rejects_wrong_engine_patch_and_missing_cooker(self):
        self.make_engine(patch_version=2)
        with self.assertRaisesRegex(ValueError, "does not match 5.8.3"):
            check_engine(self.engine, "Linux")
        self.make_engine()
        (self.engine / "Engine/Build/BatchFiles/RunUAT.sh").unlink()
        with self.assertRaisesRegex(ValueError, "RunUAT.sh"):
            check_engine(self.engine, "Linux")

    def test_rejects_malformed_engine_version(self):
        self.make_engine()
        self.write("Engine/Build/Build.version", "broken", root=self.engine)
        with self.assertRaises(ValueError):
            check_engine(self.engine, "Linux")

    def test_rejects_each_restricted_content_location(self):
        for name in PRIVATE_PATHS:
            with self.subTest(path=name):
                path = self.root / name
                path.mkdir(parents=True)
                with self.assertRaisesRegex(ValueError, "Restricted content"):
                    check_checkout(self.root)
                path.rmdir()

    def test_checks_real_git_index_and_downloaded_asset(self):
        self.write("Content/Range.umap", "downloaded map")
        subprocess.run(["git", "init", "--quiet", str(self.root)], check=True)
        subprocess.run(["git", "-C", str(self.root), "add", "--", "Content/Range.umap"], check=True)
        check_checkout(self.root)
        self.write("Content/Range.umap", "version https://git-lfs.github.com/spec/v1\n")
        with self.assertRaisesRegex(ValueError, "git lfs pull"):
            check_checkout(self.root)

    def test_rejects_local_configuration(self):
        self.write("Config/UserEngine.ini", "local override")
        with self.assertRaisesRegex(ValueError, "Local Config"):
            check_checkout(self.root)

    @patch("check_package_runner.run")
    def test_accepts_downloaded_assets_with_spaces_and_unicode(self, command):
        names = ("Content/Map.umap", "Plugins/Open/Content/tree é.uasset")
        for name in names:
            self.write(name, "downloaded asset")
        command.return_value = "\0".join(names) + "\0"
        check_checkout(self.root)
        command.assert_called_once_with(
            ["git", "ls-files", "-z", "--", "*.uasset", "*.umap"], cwd=self.root,
        )

    @patch("check_package_runner.run", return_value="Content/Map.umap\0")
    def test_rejects_pointer_and_missing_asset(self, command):
        self.write("Content/Map.umap", "version https://git-lfs.github.com/spec/v1\noid sha256:123\n")
        with self.assertRaisesRegex(ValueError, "git lfs pull"):
            check_checkout(self.root)
        (self.root / "Content/Map.umap").unlink()
        with self.assertRaises(FileNotFoundError):
            check_checkout(self.root)

    @patch("check_package_runner.run", return_value="")
    def test_rejects_checkout_without_assets(self, command):
        with self.assertRaisesRegex(ValueError, "No tracked Unreal assets"):
            check_checkout(self.root)

    @patch("check_package_runner.run")
    def test_rejects_linux_development_and_debuggame_editors(self, command):
        for name in ("UnrealEditor", "UnrealEditor-Li", "UnrealEditor-Linux-DebugGame"):
            with self.subTest(name=name):
                command.return_value = f"  123 {name}\n  456 bash\n"
                with self.assertRaisesRegex(ValueError, "Close it yourself"):
                    check_no_editor("Linux")

    @patch("check_package_runner.run")
    def test_rejects_windows_editors_and_command_editors(self, command):
        for name in ("UnrealEditor.exe", "UnrealEditor-Win64-DebugGame.exe", "UnrealEditor-Cmd.exe"):
            with self.subTest(name=name):
                command.return_value = f'"{name}","123","Console","1","10 K"\n'
                with self.assertRaisesRegex(ValueError, "PID 123"):
                    check_no_editor("Windows")

    @patch("check_package_runner.run")
    def test_accepts_machine_without_editor(self, command):
        command.return_value = "  123 ShaderCompileWo\n  456 bash\n"
        check_no_editor("Linux")
        command.return_value = '"powershell.exe","456","Console","1","10 K"\n'
        check_no_editor("Windows")

    @patch("check_package_runner.shutil.which", return_value=None)
    def test_rejects_missing_tools(self, which):
        with self.assertRaisesRegex(ValueError, "Missing git"):
            check_tools(self.engine, "Linux")

    @patch("check_package_runner.run", side_effect=ValueError("git failed"))
    @patch("check_package_runner.shutil.which", return_value="tool")
    def test_explains_missing_git_lfs(self, which, command):
        with self.assertRaisesRegex(ValueError, "Install Git LFS"):
            check_tools(self.engine, "Linux")

    @patch("check_package_runner.run")
    @patch("check_package_runner.shutil.which", return_value="tool")
    def test_checks_linux_toolchain_and_permissions(self, which, command):
        self.make_engine()
        command.side_effect = lambda args: "GNU tar" if args[0] == "tar" else "clang version 20.1.8"
        with self.assertRaisesRegex(ValueError, "not executable"):
            check_tools(self.engine, "Linux")
        (self.engine / "Engine/Binaries/Linux/UnrealEditor").chmod(0o755)
        with patch.dict(os.environ, {"LINUX_MULTIARCH_ROOT": ""}):
            with self.assertRaisesRegex(ValueError, "Clang 20.1.8"):
                check_tools(self.engine, "Linux")
            self.write("Engine/Extras/ThirdPartyNotUE/SDKs/HostLinux/Linux_x64/"
                       "v26_clang-20.1.8-rockylinux8/x86_64-unknown-linux-gnu/bin/clang++", root=self.engine)
            check_tools(self.engine, "Linux")

    @patch("check_package_runner.run")
    @patch("check_package_runner.registry_path")
    def test_checks_windows_compiler_sdk_and_framework(self, registry, command):
        program_files = self.root / "Program Files (x86)"
        self.write("Microsoft Visual Studio/Installer/vswhere.exe", root=program_files)
        visual_studio = self.root / "Visual Studio"
        self.write("VC/Tools/MSVC/14.44/bin/Hostx64/x64/cl.exe", root=visual_studio)
        command.return_value = str(visual_studio) + "\n"
        sdk = self.root / "SDK"
        for name in ("Include/10.0.26100.0/um/Windows.h", "Include/10.0.26100.0/ucrt/stdlib.h",
                     "Include/10.0.26100.0/cppwinrt/winrt/base.h", "Lib/10.0.26100.0/um/x64/kernel32.lib",
                     "Lib/10.0.26100.0/ucrt/x64/ucrt.lib", "bin/10.0.26100.0/x64/rc.exe"):
            self.write(name, root=sdk)
        netfx = self.root / "NETFX"
        registry.side_effect = lambda key, value: sdk if value == "KitsRoot10" else netfx
        self.write("System32/tar.exe")
        with patch.dict(os.environ, {"ProgramFiles(x86)": str(program_files), "SystemRoot": str(self.root)}):
            with self.assertRaisesRegex(ValueError, ".NET Framework"):
                check_windows_tools()
            self.write("Include/um/mscoree.h", root=netfx)
            check_windows_tools()
            (sdk / "Include/10.0.26100.0/cppwinrt/winrt/base.h").unlink()
            with self.assertRaisesRegex(ValueError, r"C\+\+/WinRT"):
                check_windows_tools()
            command.return_value = ""
            with self.assertRaisesRegex(ValueError, "MSVC x64"):
                check_windows_tools()

    @patch("check_package_runner.subprocess.run")
    def test_rejects_failed_native_commands(self, command):
        command.return_value = subprocess.CompletedProcess(["git"], 1, "", "failed")
        with self.assertRaisesRegex(ValueError, "git failed"):
            run(["git", "lfs", "version"])

    @patch("check_package_runner.platform.machine", return_value="x86_64")
    @patch("check_package_runner.platform.system", return_value="Linux")
    def test_missing_engine_has_clear_error(self, system, machine):
        with patch.dict(os.environ, {"UE_ROOT": ""}), patch("sys.stderr", new_callable=io.StringIO) as error:
            self.assertEqual(main([]), 1)
        self.assertIn("Set UE_ROOT", error.getvalue())

    @patch("check_package_runner.platform.system", return_value="Darwin")
    def test_rejects_unsupported_platform(self, system):
        with patch("sys.stderr", new_callable=io.StringIO) as error:
            self.assertEqual(main(["--engine-root", str(self.engine)]), 1)
        self.assertIn("Linux or Windows x64", error.getvalue())


if __name__ == "__main__":
    unittest.main()
