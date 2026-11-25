#!/bin/bash
# SPDX-License-Identifier: MIT
# Run only inside a disposable Fedora or openSUSE container, never on the host.
# Mount the repository read-only at /src, optionally a real package under /packages and a writable /reports:
#   docker run --rm -v "$PWD":/src:ro fedora:latest bash /src/Assets/Scripts/check_rpm_install.sh
#   docker run --rm -v "$PWD":/src:ro opensuse/tumbleweed bash /src/Assets/Scripts/check_rpm_install.sh
# Usage: bash /src/Assets/Scripts/check_rpm_install.sh [/packages/UOpenGolf-VERSION-Linux-x86_64.rpm]
# Fixture packages check install, upgrade, removal, dependency resolution and the in-game update's root
# installer, including its signature checks. A real package also checks that every shipped library
# resolves and the game starts.
set -euo pipefail
[[ -f /.dockerenv && $EUID -eq 0 ]] || { echo 'Use a disposable Docker container.' >&2; exit 2; }
package=${1:+$(readlink -f "$1")}

# gnupg makes the fixture signing keys; the packages themselves need only gpgv.
if command -v dnf >/dev/null; then
    pm_install() { dnf install -y "$@"; }
    pm_remove() { dnf remove -y "$@"; }
    pm_install python3 rpm-build cpio util-linux shadow-utils findutils gnupg2
elif command -v zypper >/dev/null; then
    pm_install() { zypper --non-interactive install --allow-unsigned-rpm "$@"; }
    pm_remove() { zypper --non-interactive remove "$@"; }
    pm_install python3 rpm-build cpio util-linux shadow findutils gzip gpg2
else
    echo 'Needs dnf or zypper.' >&2
    exit 2
fi
grep -E '^(PRETTY_NAME)=' /etc/os-release
# The build scripts need Python 3.10 or later; openSUSE Leap's python3 is 3.6.
python=python3
if ! python3 -c 'import sys; sys.exit(sys.version_info < (3, 10))'; then
    pm_install python311
    python=python3.11
fi
useradd --create-home --non-unique --uid 1000 player
cd /src
packages=/tmp/packages
"$python" -B Assets/Scripts/package_fixtures.py rpm "$packages" 0.0.1 0.0.2-rc.1 0.0.2
old=$packages/UOpenGolf-0.0.1-Linux-x86_64.rpm
candidate=$packages/UOpenGolf-0.0.2-rc.1-Linux-x86_64.rpm
new=$packages/UOpenGolf-0.0.2-Linux-x86_64.rpm

# Every Requires, by library name, resolves from this distribution's own repositories.
pm_install "$old"
# Expand HOME in the player's shell, not the installation shell.
# shellcheck disable=SC2016
runuser -u player -- sh -c 'mkdir -p "$HOME/.config/Epic/UOpenGolf/Saved"; echo keep > "$HOME/.config/Epic/UOpenGolf/Saved/settings-test"; uopengolf'
test -f /opt/uopengolf/UOpenGolf/Content/Paks/obsolete.pak
test -x /usr/bin/pkexec && test -x /usr/bin/gpgv
test "$(cat /opt/uopengolf/.package-format)" = rpm
test -z "$(find /opt/uopengolf -type f -perm /022 -print -quit)"
runuser -u player -- test ! -w /opt/uopengolf
rpm -V uopengolf

# In-game updates: the game's helper runs this root installer through pkexec. There is no
# polkit agent in a container, so run it directly as root.
installer=/usr/lib/uopengolf/install-update
keyring=/usr/share/uopengolf/release-signing-key.gpg
sums=$packages/SHA256SUMS
"$python" -c 'import sys, xml.etree.ElementTree as t; a = t.parse(sys.argv[1]).find("action"); assert a.find("annotate").text == sys.argv[2]' \
    /usr/share/polkit-1/actions/io.github.digitalhand.uopengolf.update.policy "$installer"
# shellcheck source=Assets/Scripts/check_install_update.sh
. Assets/Scripts/check_install_update.sh
test "$(rpm -q --qf '%{VERSION}' uopengolf)" = 0.0.1
# An expired signing key still counts while it is the key the package carries.
cp "$packages/expired-keyring.gpg" "$keyring"
refuse "$candidate" "$sums" "$sums.asc"
"$installer" "$candidate" "$sums" "$sums.expired.asc"
test "$(rpm -q --qf '%{VERSION}' uopengolf)" = 0.0.2~rc.1
# The upgrade put the release key back.
"$installer" "$new" "$sums" "$sums.asc"
test "$(rpm -q --qf '%{VERSION}' uopengolf)" = 0.0.2
test ! -e /opt/uopengolf/UOpenGolf/Content/Paks/obsolete.pak
test -f /opt/uopengolf/UOpenGolf/Content/Paks/current.pak
for refused in "$old" "$candidate" "$new"; do
    refuse "$refused" "$sums" "$sums.asc"
done
test -z "$(find /var/tmp -maxdepth 1 -name 'uopengolf-update.*' -print -quit)"

pm_remove uopengolf
test ! -e /usr/bin/uopengolf
test ! -e /opt/uopengolf
test ! -e "$installer"
test ! -e "$keyring"
test ! -e /usr/share/applications/uopengolf.desktop
test -d /usr/share/icons/hicolor
test -f /home/player/.config/Epic/UOpenGolf/Saved/settings-test

if [[ -z $package ]]; then
    echo 'PASS (fixtures): install, dependency resolution, in-game update installer, upgrade, obsolete content removal and removal.'
    exit 0
fi

pm_install "$package"
"$python" -B Assets/Scripts/check_elf_deps.py /opt/uopengolf
mkdir -p /reports
chown player /reports
result=0
runuser -u player -- timeout --kill-after=10s 60s uopengolf \
    -nullrhi -nosound -nosplash -unattended -abslog=/reports/Game.log \
    >/reports/stdout.log 2>/reports/stderr.log || result=$?
[[ $result -eq 0 || $result -eq 124 ]] || { cat /reports/stderr.log; exit "$result"; }
"$python" -B Assets/Scripts/check_package_start.py /reports/Game.log
pm_remove uopengolf
echo 'PASS: fixtures, plus the real package: dependency resolution and non-root game startup.'
