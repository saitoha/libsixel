#!/bin/sh
# Ensure inspection mode accepts high color conversion with RGBA input.
#
# Steps:
# - Read a PNG image that includes an alpha channel.
# - Run img2sixel with inspection (-I).
# - Only confirm that the command exits successfully.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

test "${HAVE_IMG2SIXEL-}" = 1 || skip_all "img2sixel is disabled in this build"

input_image="${TOP_SRCDIR}/images/pngsuite/basic/basn6a08.png"

echo "1..1"
set -v

target_txt="${ARTIFACT_LOCAL_DIR}/inspection.txt"

run_img2sixel -I "${input_image}" >"${target_txt}" || {
    fail 1 "inspection with high color and RGBA input failed"
    exit 0
}

pass 1 "inspection with high color and RGBA input exits cleanly"

exit 0
