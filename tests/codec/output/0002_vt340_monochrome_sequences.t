#!/bin/sh
# Verify VT340 monochrome control sequences are emitted.
set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"
. "${TOP_SRCDIR}/tests/lib/sh/common/tap.sh"

status=0

config_macro_defined HAVE_IMG2SIXEL || skip_all

echo "1..1"
set -v

snake_tga="${images_dir}/snake.tga"
target_sixel="${ARTIFACT_LOCAL_DIR}/vt340-mono.sixel"

if run_img2sixel -bvt340mono "${snake_tga}" >"${target_sixel}"; then
    pass 1 "VT340 monochrome control sequences emitted"
else
    fail 1 "VT340 monochrome control failed"
fi

exit "${status}"
