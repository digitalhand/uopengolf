# SPDX-License-Identifier: MIT
"""Build and inspect small RPM packages without installing on the host."""
import hashlib
import io
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest
from unittest.mock import patch

from Assets.Installer.build_rpm import (
    REQUIRES, build_package, check_work_path, files_list, main, rpm_version, spec_path, write_spec,
)
from Assets.Installer.linux_payload import (
    INSTALLER, ICON_SIZES, KEYRING, METAINFO_ID, POLKIT_ACTION, RELEASE_KEY, dearmor_public_key,
)


def query(package: Path, *arguments: str) -> str:
    return subprocess.check_output(["rpm", "-qp", "--nosignature", *arguments, str(package)], text=True,
                                   stderr=subprocess.DEVNULL)


class RpmSpecTests(unittest.TestCase):
    def test_versions(self):
        self.assertEqual(rpm_version("1.2.3"), "1.2.3")
        self.assertEqual(rpm_version("1.2.3-rc.1"), "1.2.3~rc.1")
        self.assertEqual(rpm_version("0.0.1-dev.cd5c3b8"), "0.0.1~dev.cd5c3b8")
        for invalid in ("", "v1.2.3", "1.2.3\nRequires: bad", "../1.2.3", "1.2.3+meta"):
            with self.assertRaises(ValueError):
                rpm_version(invalid)

    def test_file_entries_are_quoted_and_escaped(self):
        self.assertEqual(spec_path(Path("usr/share/a b%c")), '"/usr/share/a b%%c"')
        with self.assertRaises(ValueError):
            spec_path(Path('usr/share/a"b'))

    def test_shared_directories_are_not_owned(self):
        with tempfile.TemporaryDirectory() as directory:
            stage = Path(directory)
            for name in ("opt/uopengolf/game.pak", "usr/bin/uopengolf", "usr/lib/uopengolf/install-update",
                         "usr/share/uopengolf/release-signing-key.gpg",
                         "usr/share/doc/uopengolf/copyright", "usr/share/icons/hicolor/16x16/apps/uopengolf.png"):
                (stage / name).parent.mkdir(parents=True, exist_ok=True)
                (stage / name).write_text("x")
            self.assertEqual(files_list(stage), [
                '"/opt/uopengolf"', '"/usr/lib/uopengolf"', '"/usr/share/uopengolf"', '"/usr/share/doc/uopengolf"',
                '"/usr/bin/uopengolf"',
                '"/usr/share/icons/hicolor/16x16/apps/uopengolf.png"',
            ])

    def test_work_path_is_literal_in_the_install_script(self):
        with tempfile.TemporaryDirectory() as directory:
            stage = Path(directory) / "it's $HOME `id`" / "root"
            stage.mkdir(parents=True)
            spec = Path(directory) / "uopengolf.spec"
            write_spec(stage, spec, "1.2.3")
            install = spec.read_text().split("%install\n", 1)[1].splitlines()[0]
            # The shell reads the path back unchanged: nothing in it expands.
            words = subprocess.run(["sh", "-c", f"set -- {install.split(' || ')[0][len('cp -al '):]}; "
                                    'printf "%s\\n" "$1"'], capture_output=True, text=True, check=True).stdout
            self.assertEqual(words, f"{stage}/.\n")
        for bad in ("/tmp/a%b", "/tmp/a\nb"):
            with self.subTest(path=bad), self.assertRaises(ValueError):
                check_work_path(Path(bad))


@unittest.skipUnless(shutil.which("rpmbuild") and shutil.which("rpm"), "rpm packaging tools required")
class RpmPackageTests(unittest.TestCase):
    def setUp(self):
        directory = tempfile.TemporaryDirectory(prefix="uopengolf rpm test ")
        self.addCleanup(directory.cleanup)
        self.root = Path(directory.name)
        self.source = self.root / "source"
        self.output = self.root / "output"
        self.write("UOpenGolf/Binaries/Linux/UOpenGolf", Path("/bin/true").read_bytes())
        self.write("UOpenGolf/Content/Paks/game.pak", b"game content")
        self.write("UOpenGolf/Content/Paks/name with spaces.pak", b"more content")
        self.write("NOTICES.txt", b"engine notices")
        self.write("Engine/runtime.so", b"runtime")

    def write(self, name, data):
        path = self.source / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)
        return path

    def test_package_metadata_contents_permissions_and_checksum(self):
        self.write("UOpenGolf/Saved/Config/account.ini", b"private")
        self.write("UOpenGolf/Binaries/Linux/game.debug", b"debug")
        package = build_package(self.source, "1.2.3-rc.1", self.output)
        self.assertEqual(package.name, "UOpenGolf-1.2.3-rc.1-Linux-x86_64.rpm")
        self.assertEqual(query(package, "--qf", "%{NAME} %{VERSION} %{RELEASE} %{ARCH} %{BUILDHOST}"),
                         "uopengolf 1.2.3~rc.1 1 x86_64 uopengolf-build")
        requires = query(package, "--requires").splitlines()
        for name in REQUIRES:
            self.assertIn(name, requires)
        self.assertFalse([name for name in requires if name not in REQUIRES and not name.startswith("rpmlib(")])
        modes = dict(line.split(" ", 1)[::-1] for line in
                     query(package, "--qf", "[%{FILEMODES:octal} %{FILENAMES}\n]").splitlines())
        app = "/opt/uopengolf"
        for name in (f"{app}/LICENSE", f"{app}/NOTICES.txt", f"{app}/UOpenGolf/Content/Paks/name with spaces.pak",
                     "/usr/share/applications/uopengolf.desktop", f"/usr/share/metainfo/{METAINFO_ID}.metainfo.xml",
                     f"/{POLKIT_ACTION}", f"{app}/.package-format", f"/{KEYRING}",
                     *(f"/usr/share/icons/hicolor/{size}x{size}/apps/uopengolf.png" for size in ICON_SIZES)):
            self.assertEqual(modes[name][-4:], "0644", name)
        for name in ("/usr/bin/uopengolf", f"{app}/UOpenGolf/Binaries/Linux/UOpenGolf", "/usr/lib/uopengolf/install-update"):
            self.assertEqual(modes[name][-4:], "0755", name)
        self.assertFalse(any("/Saved/" in name or name.endswith(".debug") for name in modes))
        # Shared directories stay owned by the system.
        self.assertNotIn("/usr/bin", modes)
        self.assertNotIn("/usr/share/icons/hicolor", modes)
        owners = set(query(package, "--qf", "[%{FILEUSERNAME}:%{FILEGROUPNAME}\n]").splitlines())
        self.assertEqual(owners, {"root:root"})
        checksum = package.with_name(package.name + ".sha256").read_text()
        self.assertEqual(checksum, f"{hashlib.sha256(package.read_bytes()).hexdigest()}  {package.name}\n")
        self.assertFalse(list(self.output.glob(".uopengolf-rpm-*")))

    def test_marker_and_update_helper_contents(self):
        package = build_package(self.source, "1.2.3", self.output)
        extracted = self.root / "extracted"
        extracted.mkdir()
        cpio = subprocess.run(["rpm2cpio", str(package)], check=True, capture_output=True).stdout
        subprocess.run(["cpio", "-idm", "--quiet"], input=cpio, cwd=extracted, check=True)
        self.assertEqual((extracted / "opt/uopengolf/.package-format").read_text(), "rpm\n")
        self.assertEqual((extracted / "usr/lib/uopengolf/install-update").read_bytes(),
                         (INSTALLER / "install-update").read_bytes())
        self.assertEqual((extracted / KEYRING).read_bytes(), dearmor_public_key(RELEASE_KEY.read_text()))

    def test_command_line_success_and_failure(self):
        args = ["--package-dir", str(self.source), "--version", "1.2.3", "--output-dir", str(self.output)]
        with patch("sys.stdout", new_callable=io.StringIO):
            self.assertEqual(main(args), 0)
        (self.source / "NOTICES.txt").unlink()
        with patch("sys.stderr", new_callable=io.StringIO) as errors:
            self.assertEqual(main(args), 1)
        self.assertIn("Missing packaged file", errors.getvalue())


class RpmToolTests(unittest.TestCase):
    def test_missing_build_tool(self):
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory) / "source"
            (source / "UOpenGolf/Binaries/Linux").mkdir(parents=True)
            (source / "UOpenGolf/Binaries/Linux/UOpenGolf").write_bytes(Path("/bin/true").read_bytes())
            (source / "UOpenGolf/Content/Paks").mkdir(parents=True)
            (source / "Engine").mkdir()
            (source / "NOTICES.txt").write_text("notices")
            with patch("Assets.Installer.linux_payload.shutil.which", return_value=None):
                with self.assertRaisesRegex(ValueError, "rpmbuild"):
                    build_package(source, "1.2.3", Path(directory) / "output")


if __name__ == "__main__":
    unittest.main()
