#!/bin/sh
# Inspect ASCII PPM with diffusion and background handling.
set -eux

conversion_common_path=$(CDPATH=; cd "$(dirname "$0")/.." && pwd)/../lib/sh/conversion/common.sh
. "${conversion_common_path}"

test_name=$(basename "$0")
setup_conversion_env "${test_name}"

status=0

ensure_img2sixel_available

echo "1..1"
set -v

snake_ascii_ppm="${images_dir}/snake-ascii.ppm"
target_txt="${output_dir}/ascii-ppm-inspection.txt"

require_file "${snake_ascii_ppm}"

if run_img2sixel -I -8 -dburkes -B"#ffffffffffff" "${snake_ascii_ppm}" \
        >"${target_txt}" 2>>"${log_file}"; then
    pass 1 "ASCII PPM inspection honours diffusion"
else
    fail 1 "ASCII PPM inspection failed"
fi

exit "${status}"
