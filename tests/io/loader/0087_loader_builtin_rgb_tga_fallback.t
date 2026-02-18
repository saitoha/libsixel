#!/bin/sh
# TAP test confirming builtin loader falls back for non-indexed TGA.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build";
    exit 0
}

echo "1..1"
set -v

input_tga="${TOP_SRCDIR}/tests/data/inputs/formats/snake-tga-type2-rgb.tga"

run_img2sixel -L builtin! "${input_tga}" >/dev/null || {
    fail 1 "builtin loader RGB TGA fallback failed"
    exit 0
}

pass 1 "builtin loader falls back for non-indexed TGA"
exit 0
