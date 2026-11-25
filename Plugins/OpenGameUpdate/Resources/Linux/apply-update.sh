#!/bin/sh
# SPDX-License-Identifier: MIT
# OpenGameUpdate's Linux updater. The game starts it with /bin/sh just before it quits:
#
#   apply-update.sh --pid PID --mode archive|package --file FILE --installdir DIR --version VERSION
#                   [--pkexec PKEXEC --elevate INSTALLER --sums SUMS --signature SIGNATURE]
#                   --result RESULT -- GAME [ARGUMENTS...]
#
# It waits for the game (PID) to exit, applies the verified download FILE, writes "ok VERSION" or
# "failed REASON" to RESULT for the game's next start, then starts GAME again whether or not the update
# applied, so the player always gets a game back. While it runs, its process id is in helper.pid beside
# RESULT, so a game started meanwhile leaves the download alone.
#
#   archive  FILE is a .tar.gz holding one top-level folder. It replaces DIR; every DIR/*/Saved is kept.
#   package  PKEXEC runs INSTALLER FILE SUMS SIGNATURE, which checks the release's signed checksum list
#            and installs the system package (deb, rpm) as root.
#
# POSIX sh only: it runs on Ubuntu (dash), Fedora, openSUSE, Arch and SteamOS alike.
set -eu
trap '' HUP
cd /

pid='' mode='' file='' installdir='' version='' pkexec='' elevate='' sums='' signature='' result=''
# Seconds to wait for the game to close; tests shorten it.
wait_seconds=${OPEN_GAME_UPDATE_WAIT:-120}

usage() {
    echo "apply-update.sh: $*" >&2
    exit 2
}

while [ $# -gt 0 ]; do
    case $1 in
    --) shift; break ;;
    --pid|--mode|--file|--installdir|--version|--pkexec|--elevate|--sums|--signature|--result)
        [ $# -ge 2 ] || usage "$1 needs a value"
        case $1 in
        --pid) pid=$2 ;;
        --mode) mode=$2 ;;
        --file) file=$2 ;;
        --installdir) installdir=$2 ;;
        --version) version=$2 ;;
        --pkexec) pkexec=$2 ;;
        --elevate) elevate=$2 ;;
        --sums) sums=$2 ;;
        --signature) signature=$2 ;;
        --result) result=$2 ;;
        esac
        shift 2 ;;
    *) usage "unknown argument $1" ;;
    esac
done

case $pid in ''|*[!0-9]*) usage "--pid must be a process id" ;; esac
case $mode in archive|package) ;; *) usage "--mode must be archive or package" ;; esac
case $installdir in /?*) ;; *) usage "--installdir must be an absolute folder" ;; esac
case $file in /?*) ;; *) usage "--file must be an absolute path" ;; esac
[ -n "$result" ] || usage "--result is required"
if [ "$mode" = package ]; then
    for value in "$pkexec" "$elevate" "$sums" "$signature"; do
        case $value in /?*) ;; *) usage "a package needs absolute --pkexec, --elevate, --sums and --signature" ;; esac
    done
fi
[ $# -gt 0 ] || usage "the game to restart follows --"
installdir=${installdir%/}
pidfile="$(dirname "$result")/helper.pid"

# One line, replaced atomically, so the game never reads half of it.
write_result() {
    printf '%s\n' "$1" >"$result.tmp" 2>/dev/null && mv -f "$result.tmp" "$result" 2>/dev/null || true
}

wait_for_game() {
    waited=0
    while kill -0 "$pid" 2>/dev/null; do
        [ "$waited" -lt "$wait_seconds" ] || return 1
        sleep 1
        waited=$((waited + 1))
    done
}

install_package() {
    status=0
    output=$("$pkexec" "$elevate" "$file" "$sums" "$signature" 2>&1) || status=$?
    case $status in
    0) return 0 ;;
    # pkexec: 126 when the prompt is dismissed, 127 when it can't authorize or show one.
    126|127) reason="The password prompt was cancelled or couldn't be shown." ;;
    *) reason="The package didn't install (exit $status): $(printf '%s\n' "$output" | sed '/^[[:space:]]*$/d' | tail -n 1)" ;;
    esac
    return 1
}

# One top-level folder, no absolute paths and no "..", so --strip-components=1 lands everything in DIR.
check_archive() {
    tar -tzf "$file" 2>/dev/null | awk '
        { n = split($0, part, "/"); top = part[1]
          if (top == "" || top == "." || top == "..") bad = 1
          if (NR == 1) root = top; else if (top != root) bad = 1
          for (i = 2; i <= n; i++) if (part[i] == "..") bad = 1 }
        END { exit (bad || NR == 0) }'
}

# Moves every FROM/*/Saved (settings, logs) to the same place under TO.
move_saved() {
    for saved in "$1"/*/Saved; do
        [ -d "$saved" ] || continue
        sub=$(basename "$(dirname "$saved")")
        mkdir -p "$2/$sub" && rm -rf "$2/$sub/Saved" && mv "$saved" "$2/$sub/Saved" || return 1
    done
}

replace_folder() {
    game=$1
    parent=$(dirname "$installdir")
    name=$(basename "$installdir")
    if ! check_archive; then
        reason="The update archive has an unexpected layout."
        return 1
    fi
    if ! staging=$(mktemp -d "$parent/.$name.update.XXXXXX"); then
        reason="Couldn't create a folder beside the game."
        return 1
    fi
    chmod 755 "$staging"
    if ! tar -xzf "$file" -C "$staging" --strip-components=1 --no-same-owner; then
        rm -rf "$staging"
        reason="The update couldn't be unpacked. Check free disk space."
        return 1
    fi
    case $game in "$installdir"/*) relative=${game#"$installdir"/} ;; *) relative='' ;; esac
    if [ -n "$relative" ] && [ ! -f "$staging/$relative" ]; then
        rm -rf "$staging"
        reason="The update archive doesn't contain the game."
        return 1
    fi
    # The old folder goes inside a new private folder: a name another account could guess is never used.
    if ! aside=$(mktemp -d "$parent/.$name.old.XXXXXX"); then
        rm -rf "$staging"
        reason="Couldn't create a folder beside the game."
        return 1
    fi
    old="$aside/$name"
    if ! mv "$installdir" "$old"; then
        rm -rf "$staging" "$aside"
        reason="The game's folder couldn't be moved aside."
        return 1
    fi
    if ! move_saved "$old" "$staging" || ! mv "$staging" "$installdir"; then
        # Put everything back as it was.
        move_saved "$staging" "$old" || true
        mv "$old" "$installdir" || true
        rm -rf "$staging"
        rmdir "$aside" 2>/dev/null || true
        reason="The new version couldn't be moved into place."
        return 1
    fi
    rm -rf "$aside"
}

restart() {
    cd "$installdir" 2>/dev/null || cd /
    if command -v setsid >/dev/null 2>&1; then
        setsid "$@" </dev/null >/dev/null 2>&1 &
    else
        nohup "$@" </dev/null >/dev/null 2>&1 &
    fi
}

printf '%s\n' "$$" >"$pidfile" 2>/dev/null || true
trap 'rm -f "$pidfile"' EXIT

if ! wait_for_game; then
    # The game is still running, so it isn't restarted either.
    write_result "failed The game didn't close, so the update wasn't applied."
    exit 1
fi

reason=''
if [ "$mode" = package ]; then
    install_package && applied=yes || applied=no
else
    replace_folder "$1" && applied=yes || applied=no
fi
if [ "$applied" = yes ]; then
    write_result "ok $version"
else
    write_result "failed $reason"
    echo "apply-update.sh: $reason" >&2
fi
# The game starting next may clean up the downloads.
rm -f "$pidfile"
restart "$@"
