# SPDX-License-Identifier: MIT
"""Engine-free checks for release versions from Git refs and stamping DefaultGame.ini."""
from pathlib import Path
import contextlib
import io
import tempfile
import unittest

from project_version import main, read_project_version, stamp_version, version_for_ref


SHA = "0123abcdef0123abcdef0123abcdef0123abcdef"
INI = (
    "[/Script/EngineSettings.GeneralProjectSettings]\n"
    "ProjectName=uopengolf\n"
    "ProjectVersion=0.0.1\n"
    "\n"
    "[/Script/UnrealEd.ProjectPackagingSettings]\n"
    "+MapsToCook=(FilePath=\"/Game/UOpenGolf/Maps/UOpenGolf\")\n"
)


class VersionForRefTests(unittest.TestCase):
    def test_release_tag(self):
        self.assertEqual(version_for_ref("tag", "v1.2.3", SHA, "0.0.1"), ("1.2.3", True, False))

    def test_prerelease_tag(self):
        self.assertEqual(version_for_ref("tag", "v1.2.3-rc.1", SHA, "0.0.1"), ("1.2.3-rc.1", True, True))

    def test_rejects_tags_that_are_not_versions(self):
        for tag in ("1.2.3", "v1.2", "v01.2.3", "v1.2.3+build.5", "v1.2.3-", "v1.2.3-rc..1", "release-1", "v1.2.3 "):
            with self.subTest(tag=tag):
                with self.assertRaisesRegex(ValueError, "tag"):
                    version_for_ref("tag", tag, SHA, "0.0.1")

    def test_branch_builds_are_dev_prereleases(self):
        self.assertEqual(version_for_ref("branch", "main", SHA, "0.0.1"), ("0.0.1-dev.0123abc", False, True))

    def test_branch_build_extends_a_committed_prerelease(self):
        version, _, _ = version_for_ref("branch", "main", SHA, "0.1.0-rc.1")
        self.assertEqual(version, "0.1.0-rc.1.dev.0123abc")

    def test_rejects_bad_branch_inputs(self):
        with self.assertRaisesRegex(ValueError, "commit"):
            version_for_ref("branch", "main", "not-a-sha", "0.0.1")
        with self.assertRaisesRegex(ValueError, "ProjectVersion"):
            version_for_ref("branch", "main", SHA, "0.1")

    def test_rejects_unknown_ref_type(self):
        with self.assertRaisesRegex(ValueError, "ref type"):
            version_for_ref("pull", "7/merge", SHA, "0.0.1")


class ProjectVersionIniTests(unittest.TestCase):
    def test_reads_version(self):
        self.assertEqual(read_project_version(INI), "0.0.1")

    def test_rejects_missing_or_duplicate_version(self):
        without = INI.replace("ProjectVersion=0.0.1\n", "")
        duplicate = INI.replace("ProjectVersion=0.0.1\n", "ProjectVersion=0.0.1\nProjectVersion=0.0.2\n")
        for text in (without, duplicate):
            with self.subTest(text=text):
                with self.assertRaisesRegex(ValueError, "ProjectVersion"):
                    read_project_version(text)

    def test_ignores_version_outside_project_settings(self):
        text = INI.replace("ProjectVersion=0.0.1\n", "") + "ProjectVersion=9.9.9\n"
        with self.assertRaisesRegex(ValueError, "ProjectVersion"):
            read_project_version(text)

    def test_stamp_changes_only_the_version_line(self):
        stamped = stamp_version(INI, "1.2.3-rc.1")
        self.assertEqual(stamped, INI.replace("ProjectVersion=0.0.1", "ProjectVersion=1.2.3-rc.1"))
        self.assertEqual(read_project_version(INI), "0.0.1")

    def test_stamp_keeps_line_endings(self):
        crlf = INI.replace("\n", "\r\n")
        self.assertEqual(stamp_version(crlf, "1.2.3"), crlf.replace("ProjectVersion=0.0.1", "ProjectVersion=1.2.3"))

    def test_stamp_rejects_invalid_versions(self):
        for version in ("1.2", "v1.2.3", "1.2.3\nProjectName=evil", ""):
            with self.subTest(version=version):
                with self.assertRaisesRegex(ValueError, "version"):
                    stamp_version(INI, version)


class CommandLineTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.ini = Path(self.directory.name) / "DefaultGame.ini"
        self.ini.write_bytes(INI.encode("utf-8"))

    def run_main(self, *arguments):
        output = io.StringIO()
        with contextlib.redirect_stdout(output):
            status = main([*arguments, "--ini", str(self.ini)])
        return status, output.getvalue()

    def test_for_ref_prints_workflow_outputs(self):
        status, output = self.run_main("for-ref", "branch", "main", SHA)
        self.assertEqual(status, 0)
        self.assertEqual(output, "version=0.0.1-dev.0123abc\nis_release=false\nprerelease=true\n")

    def test_stamp_writes_the_file(self):
        status, _ = self.run_main("stamp", "1.2.3")
        self.assertEqual(status, 0)
        self.assertEqual(self.ini.read_bytes().decode("utf-8"), INI.replace("0.0.1", "1.2.3"))

    def test_invalid_input_fails_without_writing(self):
        with contextlib.redirect_stderr(io.StringIO()):
            status, _ = self.run_main("stamp", "1.2")
        self.assertEqual(status, 1)
        self.assertEqual(self.ini.read_bytes().decode("utf-8"), INI)


if __name__ == "__main__":
    unittest.main()
