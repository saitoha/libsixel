#!/bin/sh
# TAP test confirming builtin loader falls back for non-indexed TGA.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

echo "1..1"
set -v

input_tga="${TOP_SRCDIR}/tests/data/inputs/formats/snake-tga-type2-rgb.tga"

run_img2sixel -L builtin! "${input_tga}" >/dev/null || {
    echo "not ok" 1 "builtin loader RGB TGA fallback failed"
    exit 0
}

echo "ok" 1 "builtin loader falls back for non-indexed TGA"
exit 0
