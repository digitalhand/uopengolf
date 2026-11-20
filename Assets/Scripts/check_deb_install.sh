#!/bin/bash
# SPDX-License-Identifier: MIT
# Run only inside a disposable Ubuntu 24.04 container, never on the host.
# Mount the repository read-only at /src, packages at /packages and a writable /reports.
# Usage: bash /src/Assets/Scripts/check_deb_install.sh /packages/UOpenGolf-VERSION-Linux-amd64.deb
set -euo pipefail
[[ -f /.dockerenv && $EUID -eq 0 ]] || { echo 'Use a disposable Docker container.' >&2; exit 2; }
package=$(readlink -f "${1:?Supply the real .deb to test}")
export DEBIAN_FRONTEND=noninteractive
apt-get update -qq
apt-get install -y --no-install-recommends python3 desktop-file-utils binutils appstream
metainfo=/usr/share/metainfo/io.github.digitalhand.uopengolf.metainfo.xml
useradd --create-home --non-unique --uid 1000 player
mkdir -p /reports
chown player:player /reports
cd /src
python3 -B - <<'PY'
from pathlib import Path
import shutil
from Assets.Installer.build_deb import build_package

source = Path('/tmp/deb-fixture')
binary = source / 'UOpenGolf/Binaries/Linux/UOpenGolf'
binary.parent.mkdir(parents=True)
shutil.copyfile('/bin/true', binary)
(source / 'UOpenGolf/Content/Paks').mkdir(parents=True)
(source / 'Engine').mkdir()
(source / 'NOTICES.txt').write_text('Fixture notices\n')
obsolete = source / 'UOpenGolf/Content/Paks/obsolete.pak'
obsolete.write_text('old content')
build_package(source, '0.0.1', Path('/tmp/packages'))
obsolete.unlink()
(source / 'UOpenGolf/Content/Paks/current.pak').write_text('new content')
build_package(source, '0.0.2', Path('/tmp/packages'))
PY
apt-get install -y --no-install-recommends /tmp/packages/UOpenGolf-0.0.1-Linux-amd64.deb
# Expand HOME in the player's shell, not the installation shell.
# shellcheck disable=SC2016
runuser -u player -- sh -c 'mkdir -p "$HOME/.config/Epic/UOpenGolf/Saved"; echo keep > "$HOME/.config/Epic/UOpenGolf/Saved/settings-test"; uopengolf'
test -f /opt/uopengolf/UOpenGolf/Content/Paks/obsolete.pak
apt-get install -y --no-install-recommends /tmp/packages/UOpenGolf-0.0.2-Linux-amd64.deb
test ! -e /opt/uopengolf/UOpenGolf/Content/Paks/obsolete.pak
test -f /opt/uopengolf/UOpenGolf/Content/Paks/current.pak
apt-get remove -y uopengolf
test ! -e /usr/bin/uopengolf
test ! -e /usr/share/applications/uopengolf.desktop
test ! -e "$metainfo"
test -f /home/player/.config/Epic/UOpenGolf/Saved/settings-test
# With no conffiles, remove forgets the local package completely. Reinstall it
# before checking purge as a separate operation.
apt-get install -y --no-install-recommends /tmp/packages/UOpenGolf-0.0.2-Linux-amd64.deb
apt-get purge -y uopengolf
test -f /home/player/.config/Epic/UOpenGolf/Saved/settings-test

apt-get install -y --no-install-recommends "$package"
desktop-file-validate /usr/share/applications/uopengolf.desktop
# Software centres read this; Discover also needs it to name the package.
appstreamcli validate --no-net "$metainfo"
appstreamcli refresh-cache --force >/dev/null
# grep without -q reads all output, so pipefail can't see a broken pipe from appstreamcli.
appstreamcli get io.github.digitalhand.uopengolf --details | grep -x 'Package: uopengolf' >/dev/null
test -z "$(find /opt/uopengolf -type f -perm /022 -print -quit)"
runuser -u player -- test ! -w /opt/uopengolf
# Resolve every shipped ELF library against Ubuntu 24.04, not the build host.
python3 - <<'PY'
from pathlib import Path
import subprocess
for path in Path('/opt/uopengolf').rglob('*'):
    if path.is_file():
        with path.open('rb') as stream:
            elf = stream.read(4) == b'\x7fELF'
        if elf:
            result = subprocess.run(['ldd', str(path)], text=True, capture_output=True)
            if result.returncode or 'not found' in result.stdout:
                raise SystemExit(f'{path}: {result.stdout} {result.stderr}')
print('All shipped ELF dependencies resolve on Ubuntu 24.04.')
PY
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
echo 'PASS: install, upgrade, obsolete content removal, uninstall, purge, dependency resolution and non-root game startup.'
