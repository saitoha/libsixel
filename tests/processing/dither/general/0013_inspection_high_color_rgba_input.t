#!/bin/sh
# Ensure inspection mode accepts high color conversion with RGBA input.
#
# Steps:
# - Read a PNG image that includes an alpha channel.
# - Run img2sixel with inspection (-I).
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

input_image="${TOP_SRCDIR}/images/pngsuite/basic/basn6a08.png"


target_txt="${ARTIFACT_LOCAL_DIR}/inspection.txt"

run_img2sixel -I "${input_image}" >"${target_txt}" || {
    echo "not ok" 1 - "inspection with high color and RGBA input failed"
    exit 0
}

echo "ok" 1 - "inspection with high color and RGBA input exits cleanly"

exit 0
