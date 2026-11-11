#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
# Attach files to the GitHub Release for a tag, creating it as a draft first if needed.
# Used by both package workflows; needs gh with GH_TOKEN and GH_REPO (owner/repo) set.
set -euo pipefail

usage() {
    echo "Usage: bash Scripts/draft_release.sh TAG PRERELEASE(true|false) FILE..." >&2
}

[[ $# -ge 3 ]] || { usage; exit 2; }
tag="$1"
prerelease="$2"
shift 2
[[ "$prerelease" == true || "$prerelease" == false ]] || { usage; exit 2; }
[[ -n "${GH_REPO:-}" ]] || { echo 'Set GH_REPO to owner/repo.' >&2; exit 2; }

# The releases/tags/TAG endpoint hides drafts, so look the tag up among all releases.
# Prints one "true" (draft) or "false" (published) line per release with this tag.
drafts="$(gh api --paginate "repos/$GH_REPO/releases" --jq '.[] | [.tag_name, .draft] | @tsv' |
    awk -F '\t' -v tag="$tag" '$1 == tag { print $2 }')"

upload=(--clobber)
case "$drafts" in
    "")
        flags=(--draft --verify-tag --generate-notes --title "UOpenGolf $tag")
        [[ "$prerelease" == false ]] || flags+=(--prerelease)
        gh release create "$tag" "${flags[@]}"
        ;;
    true) ;;
    false)
        # Never replace a published release's files; a missing platform can still be added.
        echo "Release $tag is already published; adding files without replacing existing ones."
        upload=()
        ;;
    *)
        echo "More than one release uses $tag; delete the extra drafts and run again." >&2
        exit 1
        ;;
esac
gh release upload "$tag" "$@" ${upload[@]+"${upload[@]}"}
