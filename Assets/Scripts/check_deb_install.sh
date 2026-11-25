#!/bin/bash
# SPDX-License-Identifier: MIT
# Run only inside a disposable Ubuntu 24.04 container, never on the host.
# Mount the repository read-only at /src, optionally a real package under /packages and a writable /reports:
#   docker run --rm -v "$PWD":/src:ro ubuntu:24.04 bash /src/Assets/Scripts/check_deb_install.sh
# Usage: bash /src/Assets/Scripts/check_deb_install.sh [/packages/UOpenGolf-VERSION-Linux-amd64.deb]
# Fixture packages check install, upgrade, removal, purge and the in-game update's root installer,
# including its signature checks. A real package also checks its metadata, that every shipped library
# resolves and that the game starts.
set -euo pipefail
[[ -f /.dockerenv && $EUID -eq 0 ]] || { echo 'Use a disposable Docker container.' >&2; exit 2; }
package=${1:+$(readlink -f "$1")}
export DEBIAN_FRONTEND=noninteractive
apt-get update -qq
# gnupg makes the fixture signing keys; the packages themselves need only gpgv.
apt-get install -y --no-install-recommends python3 desktop-file-utils binutils appstream gnupg
metainfo=/usr/share/metainfo/io.github.digitalhand.uopengolf.metainfo.xml
useradd --create-home --non-unique --uid 1000 player
mkdir -p /reports
chown player:player /reports
cd /src
packages=/tmp/packages
python3 -B Assets/Scripts/package_fixtures.py deb "$packages" 0.0.1 0.0.2-rc.1 0.0.2
old=$packages/UOpenGolf-0.0.1-Linux-amd64.deb
candidate=$packages/UOpenGolf-0.0.2-rc.1-Linux-amd64.deb
new=$packages/UOpenGolf-0.0.2-Linux-amd64.deb

apt-get install -y --no-install-recommends "$old"
# Expand HOME in the player's shell, not the installation shell.
# shellcheck disable=SC2016
runuser -u player -- sh -c 'mkdir -p "$HOME/.config/Epic/UOpenGolf/Saved"; echo keep > "$HOME/.config/Epic/UOpenGolf/Saved/settings-test"; uopengolf'
test -f /opt/uopengolf/UOpenGolf/Content/Paks/obsolete.pak
apt-get install -y --no-install-recommends "$new"
test ! -e /opt/uopengolf/UOpenGolf/Content/Paks/obsolete.pak
test -f /opt/uopengolf/UOpenGolf/Content/Paks/current.pak
apt-get remove -y uopengolf
test ! -e /usr/bin/uopengolf
test ! -e /usr/share/applications/uopengolf.desktop
test ! -e "$metainfo"
test -f /home/player/.config/Epic/UOpenGolf/Saved/settings-test
# With no conffiles, remove forgets the local package completely. Reinstall it
# before checking purge as a separate operation.
apt-get install -y --no-install-recommends "$new"
apt-get purge -y uopengolf
test -f /home/player/.config/Epic/UOpenGolf/Saved/settings-test

# In-game updates: the game's helper runs this root installer through pkexec. There is no
# polkit agent in a container, so run it directly as root.
apt-get install -y --no-install-recommends "$old"
installer=/usr/lib/uopengolf/install-update
keyring=/usr/share/uopengolf/release-signing-key.gpg
sums=$packages/SHA256SUMS
test "$(cat /opt/uopengolf/.package-format)" = deb
test -x "$installer" && test -x /usr/bin/pkexec && test -x /usr/bin/gpgv && test -f "$keyring"
python3 -c 'import sys, xml.etree.ElementTree as t; a = t.parse(sys.argv[1]).find("action"); assert a.find("annotate").text == sys.argv[2]' \
    /usr/share/polkit-1/actions/io.github.digitalhand.uopengolf.update.policy "$installer"
# shellcheck source=Assets/Scripts/check_install_update.sh
. Assets/Scripts/check_install_update.sh
test "$(dpkg-query -W -f='${Version}' uopengolf)" = 0.0.1
# An expired signing key still counts while it is the key the package carries.
cp "$packages/expired-keyring.gpg" "$keyring"
refuse "$candidate" "$sums" "$sums.asc"
"$installer" "$candidate" "$sums" "$sums.expired.asc"
test "$(dpkg-query -W -f='${Version}' uopengolf)" = 0.0.2~rc.1
# The upgrade put the release key back.
"$installer" "$new" "$sums" "$sums.asc"
test "$(dpkg-query -W -f='${Version}' uopengolf)" = 0.0.2
test ! -e /opt/uopengolf/UOpenGolf/Content/Paks/obsolete.pak
for refused in "$old" "$candidate" "$new"; do
    refuse "$refused" "$sums" "$sums.asc"
done
test -z "$(find /var/tmp -maxdepth 1 -name 'uopengolf-update.*' -print -quit)"
apt-get purge -y uopengolf
test ! -e "$installer"
test ! -e "$keyring"

if [[ -z $package ]]; then
    echo 'PASS (fixtures): install, upgrade, obsolete content removal, uninstall, purge and the in-game update installer.'
    exit 0
fi

apt-get install -y --no-install-recommends "$package"
desktop-file-validate /usr/share/applications/uopengolf.desktop
# Software centres read this; Discover also needs it to name the package.
appstreamcli validate --no-net "$metainfo"
appstreamcli refresh-cache --force >/dev/null
# grep without -q reads all output, so pipefail can't see a broken pipe from appstreamcli.
appstreamcli get io.github.digitalhand.uopengolf --details | grep -x 'Package: uopengolf' >/dev/null
test -z "$(find /opt/uopengolf -type f -perm /022 -print -quit)"
runuser -u player -- test ! -w /opt/uopengolf
python3 -B Assets/Scripts/check_elf_deps.py /opt/uopengolf
result=0
runuser -u player -- timeout --kill-after=10s 60s uopengolf \
    -nullrhi -nosound -nosplash -unattended -abslog=/reports/Game.log \
    >/reports/stdout.log 2>/reports/stderr.log || result=$?
[[ $result -eq 0 || $result -eq 124 ]] || { cat /reports/stderr.log; exit "$result"; }
python3 -B Assets/Scripts/check_package_start.py /reports/Game.log
python3 -B - <<'PY'
from pathlib import Path
import re
import subprocess
from Assets.Installer.build_deb import debian_version

log = Path('/reports/Game.log').read_text(errors='replace')
match = re.search(r'Set ProjectVersion to ([^ ]+)\. Version Checksum', log)
installed = subprocess.check_output(['dpkg-query', '-W', '-f=${Version}', 'uopengolf'], text=True)
if not match or debian_version(match[1]) != installed:
    raise SystemExit('Cooked game version does not match the Debian package version.')
print(f'Cooked game version matches the package: {installed}')
PY
test -d /home/player/.config/Epic/UOpenGolf/Saved
apt-get purge -y uopengolf
test ! -e /opt/uopengolf
test -f /home/player/.config/Epic/UOpenGolf/Saved/settings-test
echo 'PASS: fixtures, plus the real package: metadata, dependency resolution and non-root game startup.'
