#!/bin/sh
# Ensure inspection mode accepts high color conversion with RGBA input.
#
# Steps:
# - Read a PNG image that includes an alpha channel.
# - Run img2sixel with inspection (-I).
# - Only confirm that the command exits successfully.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"
. "${TOP_SRCDIR}/tests/lib/sh/common/tap.sh"

status=0

config_macro_defined HAVE_IMG2SIXEL || skip_all

input_image="${images_dir}/pngsuite/basic/basn6a08.png"

echo "1..1"
set -v

target_txt="${ARTIFACT_LOCAL_DIR}/inspection.txt"

if run_img2sixel -I "${input_image}" >"${target_txt}"; then
    pass 1 "inspection with high color and RGBA input exits cleanly"
else
    fail 1 "inspection with high color and RGBA input failed"
fi

exit "${status}"
