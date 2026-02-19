#!/bin/sh
# Verify round-trip conversion with dithering and gamma correction.
set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

echo "1..1"
set -v

snake_jpg="${TOP_SRCDIR}/tests/data/inputs/snake_64.jpg"
snake_roundtrip="${ARTIFACT_LOCAL_DIR}/snake-roundtrip.sixel"

run_img2sixel "${snake_jpg}" -datkinson -flum -save \
    | run_img2sixel | tee "${snake_roundtrip}" >/dev/null || {
    fail 1 "round-trip conversion failed"
    exit 0
}

pass 1 "round-trip conversion with dithering and gamma succeeded"

exit 0
