# SPDX-License-Identifier: MIT
"""Build and inspect small Debian packages without installing on the host."""
import hashlib
import io
from pathlib import Path
import shutil
import subprocess
import tarfile
import tempfile
import unittest
from unittest.mock import patch

from Assets.Installer.build_deb import build_package, debian_version, main


@unittest.skipUnless(shutil.which("dpkg-deb"), "Debian packaging tools required")
class DebPackageTests(unittest.TestCase):
    def setUp(self):
        directory = tempfile.TemporaryDirectory(prefix="uopengolf deb test ")
        self.addCleanup(directory.cleanup)
        self.root = Path(directory.name)
        self.source = self.root / "source"
        self.output = self.root / "output"
        self.write("UOpenGolf/Binaries/Linux/UOpenGolf", Path("/bin/true").read_bytes())
        self.write("UOpenGolf/Content/Paks/game.pak", b"game content")
        self.write("NOTICES.txt", b"engine notices")
        self.write("Engine/runtime.so", b"runtime")

    def write(self, name, data):
        path = self.source / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)
        return path

    def build(self, version="1.2.3-rc.1"):
        return build_package(self.source, version, self.output)

    def test_package_metadata_contents_permissions_and_checksum(self):
        self.write("UOpenGolf/Saved/Config/account.ini", b"private")
        self.write("Engine/Saved/Logs/game.log", b"private")
        self.write("UOpenGolf/Binaries/Linux/game.debug", b"debug")
        self.write("UOpenGolf/Binaries/Linux/game.sym", b"symbols")
        package = self.build()
        self.assertEqual(package.name, "UOpenGolf-1.2.3-rc.1-Linux-amd64.deb")
        fields = subprocess.check_output([
            "dpkg-deb", "-f", str(package), "Package", "Version", "Architecture", "Depends",
        ], text=True)
        for field in ("Package: uopengolf", "Version: 1.2.3~rc.1", "Architecture: amd64", "libvulkan1"):
            self.assertIn(field, fields)
        archive = subprocess.check_output(["dpkg-deb", "--fsys-tarfile", str(package)])
        with tarfile.open(fileobj=io.BytesIO(archive)) as files:
            names = files.getnames()
            self.assertFalse(any("/Saved/" in name or name.endswith(".debug") for name in names))
            for name in ("./opt/uopengolf/LICENSE", "./opt/uopengolf/THIRD_PARTY_NOTICES.md",
                         "./opt/uopengolf/NOTICES.txt", "./opt/uopengolf/UOpenGolf/Binaries/Linux/game.sym",
                         "./usr/share/applications/uopengolf.desktop"):
                self.assertIn(name, names)
            for name in ("./usr/bin/uopengolf", "./opt/uopengolf/UOpenGolf/Binaries/Linux/UOpenGolf"):
                self.assertEqual(files.getmember(name).mode, 0o755)
            self.assertTrue(all(member.uid == member.gid == 0 for member in files))
        checksum = package.with_suffix(".deb.sha256").read_text()
        self.assertEqual(checksum, f"{hashlib.sha256(package.read_bytes()).hexdigest()}  {package.name}\n")
        self.assertTrue((self.source / "UOpenGolf/Saved/Config/account.ini").exists())

    def test_launcher_passes_arguments_and_uses_user_storage(self):
        package = self.build()
        extracted = self.root / "extracted"
        subprocess.run(["dpkg-deb", "-x", str(package), str(extracted)], check=True)
        app = extracted / "opt/uopengolf"
        binary = app / "UOpenGolf/Binaries/Linux/UOpenGolf"
        binary.write_text('#!/bin/sh\nprintf "%s\\n" "$PWD" "$@"\n')
        launcher = extracted / "usr/bin/uopengolf"
        launcher.write_text(launcher.read_text().replace("/opt/uopengolf", str(app)))
        result = subprocess.check_output([str(launcher), "argument with spaces", "-nullrhi"], text=True)
        self.assertEqual(result.splitlines(), [str(app), "UOpenGolf", "-SaveToUserDir",
                                             "argument with spaces", "-nullrhi"])
        if shutil.which("desktop-file-validate"):
            subprocess.run(["desktop-file-validate", str(extracted / "usr/share/applications/uopengolf.desktop")], check=True)

    def test_rejects_incomplete_wrong_architecture_and_external_links(self):
        binary = self.source / "UOpenGolf/Binaries/Linux/UOpenGolf"
        original = binary.read_bytes()
        binary.unlink()
        with self.assertRaisesRegex(ValueError, "UOpenGolf"):
            self.build()
        binary.write_bytes(b"not an executable")
        with self.assertRaisesRegex(ValueError, "x86-64"):
            self.build()
        binary.write_bytes(original)
        (self.source / "external").symlink_to(self.root)
        with self.assertRaisesRegex(ValueError, "link"):
            self.build()
        self.assertFalse(list(self.output.glob("*.deb")))

    def test_rejects_output_inside_source(self):
        with self.assertRaisesRegex(ValueError, "outside"):
            build_package(self.source, "1.2.3", self.source / "dist")

    def test_missing_content_and_build_tool(self):
        with patch("Assets.Installer.build_deb.shutil.which", return_value=None):
            with self.assertRaisesRegex(ValueError, "dpkg-deb"):
                self.build()
        shutil.rmtree(self.source / "UOpenGolf/Content/Paks")
        with self.assertRaisesRegex(ValueError, "Paks"):
            self.build()

    def test_command_line_success_and_failure(self):
        args = ["--package-dir", str(self.source), "--version", "1.2.3", "--output-dir", str(self.output)]
        with patch("sys.stdout", new_callable=io.StringIO):
            self.assertEqual(main(args), 0)
        (self.source / "NOTICES.txt").unlink()
        with patch("sys.stderr", new_callable=io.StringIO) as errors:
            self.assertEqual(main(args), 1)
        self.assertIn("Missing packaged file", errors.getvalue())

    def test_versions(self):
        self.assertEqual(debian_version("1.2.3"), "1.2.3")
        for version in ("1.2.3-rc.1", "1.2.3-dev.abcdef0"):
            converted = debian_version(version)
            subprocess.run(["dpkg", "--compare-versions", converted, "lt", "1.2.3"], check=True)
        for invalid in ("", "v1.2.3", "1.2.3\nDepends: bad", "../1.2.3", "1.2.3+meta"):
            with self.assertRaises(ValueError):
                debian_version(invalid)


if __name__ == "__main__":
    unittest.main()
