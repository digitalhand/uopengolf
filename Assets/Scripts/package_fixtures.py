# SPDX-License-Identifier: MIT
"""Fixture packages and a signed SHA256SUMS for the disposable-container install checks.

    python3 -B Assets/Scripts/package_fixtures.py deb|rpm OUTPUT VERSION...

OUTPUT receives:
- one fixture package per VERSION, built with a throwaway release key. The first version also ships
  obsolete.pak, which later versions remove.
- other.deb or other.rpm: a newer package that isn't named uopengolf.
- SHA256SUMS listing them all, with three signatures: SHA256SUMS.asc by the release key,
  SHA256SUMS.other.asc by an unrelated key, and SHA256SUMS.expired.asc by a key whose signing subkey has
  expired since it signed.
- expired-keyring.gpg: that expired key's keyring.

Run only inside a disposable container: it needs gpg and the format's build tool.
"""
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

PROJECT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(PROJECT))
from Assets.Installer import build_deb, build_rpm  # noqa: E402
from Assets.Installer.linux_payload import dearmor_public_key  # noqa: E402

BUILDERS = {"deb": build_deb.build_package, "rpm": build_rpm.build_package}
# The expired key is made and used at these times: its subkey expires a year after 2020-01-01.
CREATED, SIGNED = "20200101T000000", "20200601T000000"


def build_game(source: Path) -> Path:
    """The smallest tree check_package_dir accepts; /bin/true stands in for the game."""
    binary = source / "UOpenGolf/Binaries/Linux/UOpenGolf"
    binary.parent.mkdir(parents=True)
    shutil.copyfile("/bin/true", binary)
    (source / "UOpenGolf/Content/Paks").mkdir(parents=True)
    (source / "Engine").mkdir()
    (source / "NOTICES.txt").write_text("Fixture notices\n")
    (source / "UOpenGolf/Content/Paks/obsolete.pak").write_text("old content")
    return source


def gpg(home: Path, *arguments: str, time: str | None = None) -> str:
    faked = ["--faked-system-time", time] if time else []
    return subprocess.run(["gpg", "--homedir", str(home), "--batch", "--pinentry-mode", "loopback",
                           "--passphrase", "", *faked, *arguments], check=True, capture_output=True,
                          text=True).stdout


def make_key(home: Path, name: str, time: str | None = None) -> str:
    """A certify-only primary key with a separate signing subkey, as CodeSigning.md sets up the real one."""
    gpg(home, "--quick-generate-key", f"{name} <{name.lower()}@example.invalid>", "ed25519", "cert", "never",
        time=time)
    listing = gpg(home, "--list-keys", "--with-colons", name)
    fingerprint = next(line.split(":")[9] for line in listing.splitlines() if line.startswith("fpr:"))
    gpg(home, "--quick-add-key", fingerprint, "ed25519", "sign", "1y", time=time)
    return fingerprint


def build_other(package_format: str, output: Path) -> None:
    """A newer, signed-for package that isn't uopengolf, so install-update's name check is reached."""
    with tempfile.TemporaryDirectory() as directory:
        work = Path(directory)
        if package_format == "deb":
            (work / "root/DEBIAN").mkdir(parents=True)
            (work / "root/DEBIAN/control").write_text(
                "Package: other\nVersion: 9.9.9\nArchitecture: amd64\n"
                "Maintainer: Test <test@example.invalid>\nDescription: other\n")
            subprocess.run(["dpkg-deb", "--build", "--root-owner-group", str(work / "root"),
                            str(output / "other.deb")], check=True, stdout=subprocess.DEVNULL)
        else:
            spec = work / "other.spec"
            spec.write_text("Name: other\nVersion: 9.9.9\nRelease: 1\nSummary: other\nLicense: MIT\n"
                            "%description\nother\n%files\n")
            subprocess.run(["rpmbuild", "-bb", "--quiet", "--target", "x86_64",
                            "--define", f"_topdir {work / 'top'}", "--define", f"_rpmdir {output}",
                            "--define", "_rpmfilename other.rpm", str(spec)], check=True)


def write_sums(output: Path, package_format: str) -> Path:
    sums = output / "SHA256SUMS"
    listed = sorted(path.name for path in output.glob(f"*.{package_format}"))
    sums.write_text(subprocess.run(["sha256sum", *listed], cwd=output, check=True, capture_output=True,
                                   text=True).stdout)
    return sums


def main(argv: list[str]) -> int:
    if len(argv) < 3 or argv[0] not in BUILDERS:
        print(__doc__, file=sys.stderr)
        return 2
    package_format, output, versions = argv[0], Path(argv[1]).resolve(), argv[2:]
    output.mkdir(parents=True, exist_ok=True)
    home = Path(tempfile.mkdtemp(prefix="fixture-gnupg-"))
    home.chmod(0o700)
    try:
        make_key(home, "Release")
        make_key(home, "Other")
        make_key(home, "Expired", time=CREATED)
        release_key = output / "release-key.asc"
        release_key.write_text(gpg(home, "--armor", "--export", "Release"))
        (output / "expired-keyring.gpg").write_bytes(dearmor_public_key(gpg(home, "--armor", "--export", "Expired")))

        source = build_game(Path(tempfile.mkdtemp(prefix="fixture-game-")))
        for index, version in enumerate(versions):
            if index == 1:
                (source / "UOpenGolf/Content/Paks/obsolete.pak").unlink()
                (source / "UOpenGolf/Content/Paks/current.pak").write_text("new content")
            BUILDERS[package_format](source, version, output, signing_key=release_key)
        shutil.rmtree(source)
        build_other(package_format, output)

        sums = write_sums(output, package_format)
        for suffix, key, time in ((".asc", "Release", None), (".other.asc", "Other", None),
                                  (".expired.asc", "Expired", SIGNED)):
            gpg(home, "--local-user", key, "--armor", "--detach-sign", "--output", str(output / f"SHA256SUMS{suffix}"),
                str(sums), time=time)
    finally:
        subprocess.run(["gpgconf", "--homedir", str(home), "--kill", "all"], check=False)
        shutil.rmtree(home, ignore_errors=True)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
