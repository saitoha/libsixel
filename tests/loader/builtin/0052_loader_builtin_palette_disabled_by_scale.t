#!/bin/sh
# TAP test confirming scale options disable builtin palette fast path.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

echo "1..1"
set -v

input_png="${TOP_SRCDIR}/tests/data/inputs/formats/palette.png"

run_img2sixel -L builtin! -w 32 "${input_png}" >/dev/null || {
    echo "not ok" 1 "scaled indexed PNG decode failed with builtin loader"
    exit 0
}

echo "ok" 1 "scale options disable builtin palette fast path"
exit 0
