#!/bin/sh
# Verify VT340 monochrome control sequences are emitted.
set -eux

conversion_common_path="${TOP_SRCDIR}/tests/lib/sh/conversion/common.sh"
. "${conversion_common_path}"

status=0

ensure_img2sixel_available

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
