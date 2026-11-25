# SPDX-License-Identifier: MIT
"""Run OpenGameUpdate's Linux helper (apply-update.sh) against temporary folders, never the real game."""
import io
import os
from pathlib import Path
import shutil
import subprocess
import tarfile
import tempfile
import threading
import time
import unittest

PROJECT = Path(__file__).resolve().parents[2]
HELPER = PROJECT / "Plugins/OpenGameUpdate/Resources/Linux/apply-update.sh"
# dash is /bin/sh on Ubuntu and has no bashisms; bash is /bin/sh on Fedora, openSUSE and Arch.
SHELLS = [shell for shell in (shutil.which("dash"), shutil.which("bash")) if shell]
GAME = "UOpenGolf/Binaries/Linux/UOpenGolf"


class ApplyUpdateTests(unittest.TestCase):
    def setUp(self):
        directory = tempfile.TemporaryDirectory(prefix="apply update ")
        self.addCleanup(directory.cleanup)
        self.root = Path(directory.name)
        self.games = self.root / "My Games"
        self.install = self.games / "UOpenGolf-0.2.0-Linux-x64"
        self.result = self.root / "cache/result"
        self.result.parent.mkdir(parents=True)
        self.restarted = self.root / "restarted"
        self.bin = self.root / "bin"
        self.bin.mkdir()
        self.write(self.install / GAME, self.game_script("old"), executable=True)
        self.write(self.install / "UOpenGolf/Content/Paks/old.pak", "old content")
        self.write(self.install / "UOpenGolf/Saved/Config/Game.ini", "player settings")
        self.write(self.install / "Engine/Saved/Logs/crash.log", "engine log")

    def game_script(self, version: str) -> str:
        """The "game" records its version, working folder and arguments when the helper restarts it."""
        return f'#!/bin/sh\nprintf "%s\\n" "{version}" "$PWD" "$@" > "{self.restarted}"\n'

    def installed_version(self) -> str:
        return (self.install / GAME).read_text().split('"')[3]

    @staticmethod
    def write(path: Path, text: str, executable: bool = False) -> Path:
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text)
        path.chmod(0o755 if executable else 0o644)
        return path

    def archive(self, members: dict[str, str], name: str = "release.tar.gz") -> Path:
        path = self.root / name
        with tarfile.open(path, "w:gz") as archive:
            for member, text in members.items():
                data = text.encode()
                info = tarfile.TarInfo(member)
                info.size = len(data)
                info.mode = 0o755 if member.endswith("UOpenGolf") else 0o644
                archive.addfile(info, io.BytesIO(data))
        return path

    def release(self) -> Path:
        top = "UOpenGolf-0.3.0-Linux-x64"
        return self.archive({f"{top}/{GAME}": self.game_script("new"), f"{top}/UOpenGolf/Content/Paks/new.pak": "new content",
                             f"{top}/LICENSE": "licence"})

    def run_helper(self, mode: str, file: Path, shell: str, pid: int | None = None, extra=(), env=None):
        arguments = [shell, str(HELPER), "--pid", str(pid or self.exited_pid()), "--mode", mode,
                     "--file", str(file), "--installdir", str(self.install) + "/", "--version", "0.3.0",
                     *extra, "--result", str(self.result), "--", str(self.install / GAME), "UOpenGolf", "-a b"]
        environment = {**os.environ, "PATH": f"{self.bin}:{os.environ['PATH']}", "OPEN_GAME_UPDATE_WAIT": "3",
                       **(env or {})}
        completed = subprocess.run(arguments, env=environment, capture_output=True, text=True, timeout=30)
        return completed

    @staticmethod
    def exited_pid() -> int:
        process = subprocess.Popen(["true"])
        process.wait()
        return process.pid

    def wait_for_restart(self) -> list[str]:
        deadline = time.monotonic() + 5
        while not self.restarted.exists() and time.monotonic() < deadline:
            time.sleep(0.05)
        self.assertTrue(self.restarted.exists(), "game was not restarted")
        time.sleep(0.05)
        return self.restarted.read_text().splitlines()

    def test_archive_replaces_folder_and_keeps_saved(self):
        for shell in SHELLS:
            with self.subTest(shell=shell):
                self.setUp()
                completed = self.run_helper("archive", self.release(), shell)
                self.assertEqual(completed.returncode, 0, completed.stderr)
                self.assertEqual(self.installed_version(), "new")
                self.assertTrue(os.access(self.install / GAME, os.X_OK))
                self.assertTrue((self.install / "UOpenGolf/Content/Paks/new.pak").exists())
                self.assertFalse((self.install / "UOpenGolf/Content/Paks/old.pak").exists())
                self.assertEqual((self.install / "UOpenGolf/Saved/Config/Game.ini").read_text(), "player settings")
                self.assertEqual((self.install / "Engine/Saved/Logs/crash.log").read_text(), "engine log")
                self.assertEqual(self.install.stat().st_mode & 0o777, 0o755)
                self.assertEqual(sorted(path.name for path in self.games.iterdir()), [self.install.name])
                self.assertEqual(self.result.read_text(), "ok 0.3.0\n")
                self.assertEqual(self.wait_for_restart(), ["new", str(self.install), "UOpenGolf", "-a b"])

    def test_rejects_unsafe_or_unexpected_archives(self):
        cases = {
            "escape": {"UOpenGolf-0.3.0/../../evil": "x", f"UOpenGolf-0.3.0/{GAME}": self.game_script("new")},
            "absolute": {"/tmp/evil": "x"},
            "two roots": {f"a/{GAME}": self.game_script("new"), "b/file": "x"},
            "no game": {"UOpenGolf-0.3.0/LICENSE": "licence"},
            "not an archive": None,
        }
        for label, members in cases.items():
            with self.subTest(case=label):
                self.setUp()
                if members is None:
                    file = self.write(self.root / "broken.tar.gz", "not gzip")
                else:
                    file = self.archive(members, f"{label}.tar.gz")
                completed = self.run_helper("archive", file, SHELLS[0])
                self.assertEqual(completed.returncode, 0, completed.stderr)
                self.assertTrue(self.result.read_text().startswith("failed "), self.result.read_text())
                self.assertEqual(self.installed_version(), "old")
                self.assertEqual((self.install / "UOpenGolf/Saved/Config/Game.ini").read_text(), "player settings")
                self.assertEqual(sorted(path.name for path in self.games.iterdir()), [self.install.name])
                self.assertFalse((self.root / "evil").exists())
                self.assertEqual(self.wait_for_restart(), ["old", str(self.install), "UOpenGolf", "-a b"])

    def test_failed_swap_rolls_back(self):
        # A stub mv that refuses only the final move of the new folder into place.
        self.write(self.bin / "mv", (
            '#!/bin/sh\ncase $1 in */.UOpenGolf-0.2.0-Linux-x64.update.??????) echo "mv: refused" >&2; exit 1 ;; esac\n'
            'exec /bin/mv "$@"\n'), executable=True)
        completed = self.run_helper("archive", self.release(), SHELLS[0])
        self.assertEqual(completed.returncode, 0, completed.stderr)
        self.assertEqual(self.result.read_text(), "failed The new version couldn't be moved into place.\n")
        self.assertEqual(self.installed_version(), "old")
        self.assertEqual((self.install / "UOpenGolf/Saved/Config/Game.ini").read_text(), "player settings")
        self.assertEqual((self.install / "Engine/Saved/Logs/crash.log").read_text(), "engine log")
        self.assertEqual(sorted(path.name for path in self.games.iterdir()), [self.install.name])
        self.assertEqual(self.wait_for_restart()[0], "old")

    def test_package_uses_pkexec_and_reports_its_result(self):
        package = self.write(self.root / "cache/0.3.0/UOpenGolf-0.3.0-Linux-amd64.deb", "package")
        sums = self.write(self.root / "cache/0.3.0/SHA256SUMS", "sums")
        signature = self.write(self.root / "cache/0.3.0/SHA256SUMS.asc", "signature")
        calls = self.root / "pkexec-calls"
        # The exact pkexec the game checked, not whichever one PATH finds first.
        pkexec = self.root / "sbin/pkexec"
        cases = {
            "0": "ok 0.3.0",
            "126": "failed The password prompt was cancelled or couldn't be shown.",
            "100": "failed The package didn't install (exit 100): E: Unable to locate package",
        }
        for status, expected in cases.items():
            with self.subTest(status=status):
                self.restarted.unlink(missing_ok=True)
                self.write(pkexec, (
                    f'#!/bin/sh\nprintf "%s\\n" "$@" > "{calls}"\necho "Reading package lists..."\n'
                    f'echo "E: Unable to locate package"\necho\nexit {status}\n'), executable=True)
                completed = self.run_helper("package", package, SHELLS[0], extra=(
                    "--pkexec", str(pkexec), "--elevate", "/usr/lib/uopengolf/install-update",
                    "--sums", str(sums), "--signature", str(signature)))
                self.assertEqual(completed.returncode, 0, completed.stderr)
                self.assertEqual(self.result.read_text(), expected + "\n")
                self.assertEqual(calls.read_text().splitlines(),
                                 ["/usr/lib/uopengolf/install-update", str(package), str(sums), str(signature)])
                self.wait_for_restart()

    def test_waits_for_the_game_and_gives_up_without_restarting(self):
        game = subprocess.Popen(["sleep", "30"])
        self.addCleanup(game.wait)
        self.addCleanup(game.kill)
        pidfile = self.result.parent / "helper.pid"
        recorded = []
        # While it waits, its process id is in helper.pid, so a game started meanwhile keeps the download.
        watcher = threading.Thread(target=lambda: recorded.append(self.wait_for_file(pidfile)))
        watcher.start()
        started = time.monotonic()
        completed = self.run_helper("archive", self.release(), SHELLS[0], pid=game.pid)
        watcher.join()
        self.assertGreaterEqual(time.monotonic() - started, 3)
        self.assertEqual(completed.returncode, 1)
        self.assertTrue(recorded[0].strip().isdigit(), recorded)
        self.assertFalse(pidfile.exists())
        self.assertEqual(self.result.read_text(), "failed The game didn't close, so the update wasn't applied.\n")
        self.assertEqual(self.installed_version(), "old")
        time.sleep(0.2)
        self.assertFalse(self.restarted.exists())

    @staticmethod
    def wait_for_file(path: Path) -> str:
        deadline = time.monotonic() + 5
        while time.monotonic() < deadline:
            if path.exists() and path.read_text():
                return path.read_text()
            time.sleep(0.02)
        return ""

    def test_applies_once_the_game_exits(self):
        game = subprocess.Popen(["sleep", "1"])
        # Reap it as soon as it exits, as the game's own parent would; a zombie still answers kill -0.
        threading.Thread(target=game.wait).start()
        completed = self.run_helper("archive", self.release(), SHELLS[0], pid=game.pid)
        self.assertEqual(completed.returncode, 0, completed.stderr)
        self.assertEqual(self.result.read_text(), "ok 0.3.0\n")
        self.assertEqual(self.wait_for_restart()[0], "new")
        self.assertFalse((self.result.parent / "helper.pid").exists())

    def test_rejects_bad_arguments(self):
        base = [SHELLS[0], str(HELPER), "--pid", "1", "--file", "/tmp/x", "--installdir", str(self.install),
                "--result", str(self.result)]
        for arguments in (
            [*base, "--mode", "archive"],                                   # no game to restart
            [*base, "--mode", "package", "--", "game"],                     # package without --elevate
            [*base, "--mode", "package", "--pkexec", "pkexec", "--elevate", "/i", "--sums", "/s",
             "--signature", "/g", "--", "game"],                            # relative pkexec
            [*base, "--mode", "other", "--", "game"],
            [SHELLS[0], str(HELPER), "--pid", "x", "--mode", "archive", "--", "game"],
            [SHELLS[0], str(HELPER), "--unknown"],
        ):
            with self.subTest(arguments=arguments[2:]):
                completed = subprocess.run(arguments, capture_output=True, text=True, timeout=10)
                self.assertEqual(completed.returncode, 2)
        self.assertFalse(self.result.exists())


if __name__ == "__main__":
    unittest.main()
