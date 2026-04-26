#!/bin/sh
# Emit TAP ensuring legacy dither policy .inc.h fragments are gone.

set -eu

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

find "$src_root/src" -maxdepth 1 -type f -name 'dither-policy-*.inc.h' \
    -print | LC_ALL=C sort >> "$bad"

find "$src_root/src" -maxdepth 1 -type f -name 'dither-policy-*.c' \
    -print | LC_ALL=C sort | while IFS= read -r path
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
