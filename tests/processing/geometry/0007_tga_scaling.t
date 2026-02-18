#!/bin/sh
# Validate TGA conversion with scaling and palette options.
set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build";
    exit 0
}

echo "1..1"
set -v

snake_tga="${TOP_SRCDIR}/tests/data/inputs/snake_64.tga"
target_sixel="${ARTIFACT_LOCAL_DIR}/snake-tga.sixel"

run_img2sixel -7 -sauto -w100 -rga -qauto -dburkes -tauto \
    "${snake_tga}" >"${target_sixel}" || {
    fail 1 "TGA conversion with scaling failed"
    exit 0
}

pass 1 "TGA conversion with scaling succeeded"

exit 0
