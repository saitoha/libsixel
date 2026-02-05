#!/bin/sh
# Inspect ASCII PPM with diffusion and background handling.
set -eux

conversion_common_path="${TOP_SRCDIR}/tests/lib/sh/conversion/common.sh"
. "${conversion_common_path}"

status=0

ensure_img2sixel_available

echo "1..1"
set -v

snake_ascii_ppm="${images_dir}/snake-ascii.ppm"
target_txt="${ARTIFACT_LOCAL_DIR}/ascii-ppm-inspection.txt"

if ! run_img2sixel -I -8 -dburkes -B"#ffffffffffff" "${snake_ascii_ppm}" >"${target_txt}"; then
    fail 1 "ASCII PPM inspection failed"
    exit "${status}"
fi

pass 1 "ASCII PPM inspection honours diffusion"
exit "${status}"
