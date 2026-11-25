# SPDX-License-Identifier: MIT
"""Shared .deb/.rpm staging: the release key that install-update checks signatures with."""
import base64
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path

from Assets.Installer.linux_payload import (
    KEYRING, PACKAGE_FORMAT, RELEASE_KEY, UPDATE_INSTALLER, crc24, dearmor_public_key, stage_updater,
)


def armor(data: bytes, checksum: int | None = None) -> str:
    """An ASCII-armored public key block around data, as gpg --armor --export writes it."""
    body = base64.b64encode(data).decode()
    lines = [body[i:i + 64] for i in range(0, len(body), 64)]
    crc = base64.b64encode((crc24(data) if checksum is None else checksum).to_bytes(3, "big")).decode()
    return "\n".join(["-----BEGIN PGP PUBLIC KEY BLOCK-----", "Comment: test", "", *lines, f"={crc}",
                      "-----END PGP PUBLIC KEY BLOCK-----", ""])


class DearmorTests(unittest.TestCase):
    def test_round_trip_and_checksum(self):
        data = b"\x99\x00\x33" + bytes(range(51))
        self.assertEqual(dearmor_public_key(armor(data)), data)
        with self.assertRaisesRegex(ValueError, "checksum"):
            dearmor_public_key(armor(data, checksum=crc24(data) ^ 1))

    def test_rejects_anything_but_a_public_key(self):
        for text in ("", "not armored", armor(b"\x88\x01\x02"),  # a signature packet, not a key
                     armor(b"\x99\x00").replace("PUBLIC KEY", "PRIVATE KEY")):
            with self.subTest(text=text[:40]), self.assertRaises(ValueError):
                dearmor_public_key(text)

    @unittest.skipUnless(shutil.which("gpg"), "gpg required")
    def test_release_key_matches_gpg_dearmor(self):
        expected = subprocess.run(["gpg", "--dearmor"], input=RELEASE_KEY.read_bytes(), check=True,
                                  capture_output=True).stdout
        self.assertEqual(dearmor_public_key(RELEASE_KEY.read_text()), expected)

    def test_stage_updater_uses_the_given_key(self):
        data = b"\x99\x00\x02ab"
        with tempfile.TemporaryDirectory() as directory:
            stage = Path(directory) / "stage"
            key = Path(directory) / "key.asc"
            key.write_text(armor(data))
            stage_updater(stage, "rpm", key)
            self.assertEqual((stage / KEYRING).read_bytes(), data)
            self.assertEqual((stage / PACKAGE_FORMAT).read_text(), "rpm\n")
            self.assertTrue((stage / UPDATE_INSTALLER).is_file())
            stage_updater(Path(directory) / "default", "deb")
            self.assertEqual((Path(directory) / "default" / KEYRING).read_bytes(),
                             dearmor_public_key(RELEASE_KEY.read_text()))
            with self.assertRaises(ValueError):
                stage_updater(Path(directory) / "other", "pacman", key)


if __name__ == "__main__":
    unittest.main()
