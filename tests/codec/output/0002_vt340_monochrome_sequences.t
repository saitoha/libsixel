#!/bin/sh
# Verify VT340 monochrome control sequences are emitted.
set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

status=0

config_macro_defined HAVE_IMG2SIXEL || skip_all "img2sixel is disabled in this build"

echo "1..1"
set -v

snake_tga="${top_srcdir}/tests/data/inputs/snake_64.tga"
target_sixel="${ARTIFACT_LOCAL_DIR}/vt340-mono.sixel"

if run_img2sixel -bvt340mono "${snake_tga}" >"${target_sixel}"; then
    pass 1 "VT340 monochrome control sequences emitted"
else
    fail 1 "VT340 monochrome control failed"
fi

exit "${status}"
