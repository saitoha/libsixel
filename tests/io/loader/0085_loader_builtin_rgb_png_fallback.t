#!/bin/sh
# TAP test confirming builtin loader falls back for non-indexed PNG.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

echo "1..1"
set -v

input_png="${TOP_SRCDIR}/tests/data/inputs/formats/rgb.png"

run_img2sixel -L builtin! "${input_png}" >/dev/null || {
    fail 1 "builtin loader RGB PNG fallback failed"
    exit 0
}

pass 1 "builtin loader falls back for non-indexed PNG"
exit 0
