#!/bin/sh
# Verify VT340 colour control sequences are emitted.
set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"
. "${TOP_SRCDIR}/tests/lib/sh/common/tap.sh"

status=0

config_macro_defined HAVE_IMG2SIXEL || skip_all

echo "1..1"
set -v

snake_ppm="${images_dir}/snake.ppm"
target_sixel="${ARTIFACT_LOCAL_DIR}/vt340-colour.sixel"

if run_img2sixel -bvt340color "${snake_ppm}" >"${target_sixel}"; then
    pass 1 "VT340 colour control sequences emitted"
else
    fail 1 "VT340 colour control emission failed"
fi

exit "${status}"
