#!/bin/sh
# Emit TAP ensuring legacy dither policy .inc.h fragments are gone.

set -eu
source_files_helper=$(CDPATH='' cd -- "${0%/*}" && pwd -P)/list-source-files.sh

src_root=${1:-}

echo "1..1"

if test -z "$src_root"; then
    echo "not ok 1 - dither policy include fragments are removed"
    echo "# src_root argument is required"
    exit 1
fi

tmpdir=$(mktemp -d "${TMPDIR:-/tmp}/libsixel-dither-inc-removed-XXXXXX")
trap 'rm -rf "$tmpdir"' EXIT HUP INT TERM

bad=$tmpdir/bad.txt
: > "$bad"

sh "$source_files_helper" "$src_root/src" --max-depth 1 --name 'dither-policy-*.inc.h'| LC_ALL=C sort >> "$bad"

sh "$source_files_helper" "$src_root/src" --max-depth 1 --name 'dither-policy-*.c'| LC_ALL=C sort | while IFS= read -r path
 do
    awk '
    /#include "dither-policy-.*\.inc\.h"/ {
        printf "%s:%d:%s\n", FILENAME, NR, $0
    }
    ' "$path"
 done >> "$bad"

if test -s "$bad"; then
    echo "not ok 1 - dither policy include fragments are removed"
    sed 's/^/# /' "$bad"
    exit 1
fi

echo "ok 1 - dither policy include fragments are removed"
exit 0
