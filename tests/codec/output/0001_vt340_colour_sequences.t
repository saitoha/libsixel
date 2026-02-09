#!/bin/sh
# Verify VT340 colour control sequences are emitted.
set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

status=0

config_macro_defined HAVE_IMG2SIXEL || skip_all "img2sixel is disabled in this build"

echo "1..1"
set -v

snake_ppm="${top_srcdir}/tests/data/inputs/small.ppm"
target_sixel="${ARTIFACT_LOCAL_DIR}/vt340-colour.sixel"

if run_img2sixel -bvt340color "${snake_ppm}" >"${target_sixel}"; then
    pass 1 "VT340 colour control sequences emitted"
else
    fail 1 "VT340 colour control emission failed"
fi

exit "${status}"
