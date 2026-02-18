#!/bin/sh
# Verify VT340 monochrome control sequences are emitted.
set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build";
    exit 0
}

echo "1..1"
set -v

snake_tga="${TOP_SRCDIR}/tests/data/inputs/snake_64.tga"
target_sixel="${ARTIFACT_LOCAL_DIR}/vt340-mono.sixel"

run_img2sixel -bvt340mono "${snake_tga}" >"${target_sixel}" || {
    fail 1 "VT340 monochrome control failed"
    exit 0
}

pass 1 "VT340 monochrome control sequences emitted"

exit 0
