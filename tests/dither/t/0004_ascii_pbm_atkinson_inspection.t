#!/bin/sh
# Inspect ASCII PBM with Atkinson diffusion.
set -eux

conversion_common_path=$(CDPATH=; cd "$(dirname "$0")/.." && pwd)/../lib/sh/conversion/common.sh
. "${conversion_common_path}"

test_name=$(basename "$0")
setup_conversion_env "${test_name}"

status=0

ensure_img2sixel_available

echo "1..1"

snake_ascii_pbm="${images_dir}/snake-ascii.pbm"
target_txt="${output_dir}/ascii-pbm-inspection.txt"

require_file "${snake_ascii_pbm}"

if run_img2sixel -I -datkinson "${snake_ascii_pbm}" \
        >"${target_txt}" 2>>"${log_file}"; then
    pass 1 "ASCII PBM Atkinson inspection works"
else
    fail 1 "ASCII PBM Atkinson inspection fails"
fi

exit "${status}"
