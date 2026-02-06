#!/bin/sh
# Verify round-trip conversion with dithering and gamma correction.
set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"
. "${TOP_SRCDIR}/tests/lib/sh/common/tap.sh"

status=0

config_macro_defined HAVE_IMG2SIXEL || skip_all "img2sixel is disabled in this build"

echo "1..1"
set -v

snake_jpg="${TOP_SRCDIR}/tests/data/inputs/snake_64.jpg"
snake_roundtrip="${ARTIFACT_LOCAL_DIR}/snake-roundtrip.sixel"

if run_img2sixel "${snake_jpg}" -datkinson -flum -save \
    | run_img2sixel | tee "${snake_roundtrip}" >/dev/null; then
    pass 1 "round-trip conversion with dithering and gamma succeeded"
else
    fail 1 "round-trip conversion failed"
fi

exit "${status}"
