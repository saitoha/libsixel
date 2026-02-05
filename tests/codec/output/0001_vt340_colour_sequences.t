#!/bin/sh
# Verify VT340 colour control sequences are emitted.
set -eux

conversion_common_path="${TOP_SRCDIR}/tests/lib/sh/conversion/common.sh"
. "${conversion_common_path}"

setup_conversion_env "${test_name}"

status=0

ensure_img2sixel_available

echo "1..1"
set -v

snake_ppm="${images_dir}/snake.ppm"
target_sixel="${output_dir}/vt340-colour.sixel"

require_file "${snake_ppm}"

if run_img2sixel -bvt340color "${snake_ppm}" \
        >"${target_sixel}"; then
    pass 1 "VT340 colour control sequences emitted"
else
    fail 1 "VT340 colour control emission failed"
fi

exit "${status}"
