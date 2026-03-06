#!/bin/sh
# Ensure inspection mode accepts high color conversion with Sierra-2.
#
# Steps:
# - Read a standard RGB test image.
# - Run img2sixel with inspection (-I) and -d sierra2.
# - Only confirm that the command exits successfully.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

echo "1..1"
set -v
mkdir -p "${ARTIFACT_LOCAL_DIR}"

input_image="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"
target_txt="${ARTIFACT_LOCAL_DIR}/inspection.txt"

run_img2sixel -I -d sierra2 "${input_image}" >"${target_txt}" || {
    echo "not ok" 1 - "inspection with high color and Sierra-2 failed"
    exit 0
}

echo "ok" 1 - "inspection with high color and Sierra-2 exits cleanly"

exit 0
