#!/bin/sh
# Perform two-pass Sixel conversion to validate re-encoding path.
set -eux

conversion_common_path=$(CDPATH=; cd "$(dirname "$0")/.." && pwd)/../lib/sh/conversion/common.sh
. "${conversion_common_path}"

test_name=$(basename "$0")
setup_conversion_env "${test_name}"

status=0

ensure_img2sixel_available

echo "1..1"

snake_png="${images_dir}/snake.png"
stage1="${output_dir}/two-pass-stage1.sixel"
stage2="${output_dir}/two-pass-stage2.sixel"

require_file "${snake_png}"

if run_img2sixel -w204 -h204 "${snake_png}" \
        >"${stage1}" 2>>"${log_file}" && \
        run_img2sixel <"${stage1}" >"${stage2}" 2>>"${log_file}"; then
    pass 1 "two-pass Sixel conversion succeeds"
else
    fail 1 "two-pass Sixel conversion fails"
fi

exit "${status}"
