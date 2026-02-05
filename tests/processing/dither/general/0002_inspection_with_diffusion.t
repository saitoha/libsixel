#!/bin/sh
# Check inspection mode with diffusion and background colour.
set -eux

conversion_common_path="${TOP_SRCDIR}/tests/lib/sh/conversion/common.sh"
. "${conversion_common_path}"

status=0

ensure_img2sixel_available

echo "1..1"
set -v

snake_ppm="${images_dir}/snake.ppm"

target_txt="${ARTIFACT_LOCAL_DIR}/inspection.txt"

if ! run_img2sixel -I -dstucki -thls -B"#a0B030" "${snake_ppm}" >"${target_txt}"; then
    fail 1 "inspection with diffusion failed"
    exit "${status}"
fi

pass 1 "inspection with diffusion and background works"
exit "${status}"
