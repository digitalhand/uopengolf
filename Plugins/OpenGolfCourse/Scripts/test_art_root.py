# SPDX-License-Identifier: MIT
import unittest
from pathlib import Path

from art_root import DEFAULT, ENVIRONMENT, art_root, script_arguments


class ArtRootTests(unittest.TestCase):
    def test_blender_arguments_follow_the_separator(self):
        self.assertEqual(script_arguments(["blender", "--python", "x.py", "--", "--regenerate"]), ["--regenerate"])
        self.assertEqual(script_arguments(["python3", "x.py"]), [])

    def test_argument_wins_over_environment(self):
        argv = ["blender", "--", "--art-root", "/tmp/art", "--regenerate"]
        self.assertEqual(art_root(argv, {ENVIRONMENT: "/srv/other"}), Path("/tmp/art"))

    def test_environment_wins_over_default(self):
        self.assertEqual(art_root(["blender"], {ENVIRONMENT: "/srv/art"}), Path("/srv/art"))

    def test_default_is_under_the_home_folder(self):
        self.assertEqual(art_root([], {}), DEFAULT.expanduser())
        self.assertTrue(art_root([], {}).is_absolute())

    def test_missing_value_is_an_error(self):
        with self.assertRaises(ValueError):
            art_root(["blender", "--", "--art-root"], {})


if __name__ == "__main__":
    unittest.main()
