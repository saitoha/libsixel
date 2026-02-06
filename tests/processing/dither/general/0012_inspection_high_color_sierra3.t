#!/bin/sh
# Ensure inspection mode accepts high color conversion with Sierra-3.
#
# Steps:
# - Read a standard RGB test image.
# - Run img2sixel with inspection (-I) and -d sierra3.
# - Only confirm that the command exits successfully.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"
. "${TOP_SRCDIR}/tests/lib/sh/common/tap.sh"

status=0

config_macro_defined HAVE_IMG2SIXEL || skip_all "img2sixel is disabled in this build"

echo "1..1"
set -v

input_image="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"
target_txt="${ARTIFACT_LOCAL_DIR}/inspection.txt"

if run_img2sixel -I -d sierra3 "${input_image}" \
        >"${target_txt}"; then
    pass 1 "inspection with high color and Sierra-3 exits cleanly"
else
    fail 1 "inspection with high color and Sierra-3 failed"
fi

exit "${status}"
