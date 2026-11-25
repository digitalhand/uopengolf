# SPDX-License-Identifier: MIT
# Sourced by check_deb_install.sh and check_rpm_install.sh, inside their disposable containers, once a
# fixture package is installed. Expects installer, packages (package_fixtures.py's output), sums and new.
# Runs every case install-update must refuse, whatever the package format, and defines refuse.
# shellcheck shell=bash
: "${installer:?}" "${packages:?}" "${sums:?}" "${new:?}"

refuse() {
    if "$installer" "$@" 2>/tmp/refused.log; then
        echo "install-update accepted: $*" >&2
        exit 1
    fi
    echo "Refused as expected: $(tail -n 1 /tmp/refused.log)"
}

format=$(cat /opt/uopengolf/.package-format)
cases=/tmp/refuse
rm -rf "$cases"
mkdir -p "$cases/tampered"
ln -s "$new" "$cases/link.$format"
mkfifo "$cases/fifo.$format"
cp "$new" "$cases/tampered/"
printf x >>"$cases/tampered/$(basename "$new")"
cp "$new" "$cases/renamed.$format"
{ cat "$sums"; printf '%064d  extra\n' 0; } >"$cases/SHA256SUMS"

refuse "relative/UOpenGolf.$format" "$sums" "$sums.asc"
refuse "$cases/link.$format" "$sums" "$sums.asc"
refuse "$cases/fifo.$format" "$sums" "$sums.asc"
# Not the signed bytes, or not under a signed name.
refuse "$cases/tampered/$(basename "$new")" "$sums" "$sums.asc"
refuse "$cases/renamed.$format" "$sums" "$sums.asc"
# Signed for, but another package.
refuse "$packages/other.$format" "$sums" "$sums.asc"
# Signed by another key, by a key this package doesn't carry, changed after signing, or not signed.
refuse "$new" "$sums" "$sums.other.asc"
refuse "$new" "$sums" "$sums.expired.asc"
refuse "$new" "$cases/SHA256SUMS" "$sums.asc"
refuse "$new" "$sums" "$sums"
refuse "$new" "$sums"
test -z "$(find /var/tmp -maxdepth 1 -name 'uopengolf-update.*' -print -quit)"
