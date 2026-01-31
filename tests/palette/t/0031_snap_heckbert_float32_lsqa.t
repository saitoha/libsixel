#!/bin/sh
# Run lsqa quality checks for Heckbert palette snapping with float32 palettes.
set -eux

conversion_common_path=$(CDPATH=; cd "$(dirname "$0")/.." && pwd)/../lib/sh/conversion/common.sh
. "${conversion_common_path}"

lsqa_common_path=$(CDPATH=; cd "$(dirname "$0")/.." && pwd)/../lib/sh/lsqa/lsqa_common.sh
LSQA_HELPER_DIR=$(CDPATH=; cd "$(dirname "${lsqa_common_path}")" && pwd)
export LSQA_HELPER_DIR
. "${lsqa_common_path}"

test_name=$(basename "$0")
setup_conversion_env "${test_name}"

status=0

lsqa_floor=${LSQA_MS_SSIM_FLOOR:-0.98}

ensure_img2sixel_available

echo "1..1"
set -v

input_image="${top_srcdir}/tests/data/inputs/snake_64.png"
output_sixel="${artifact_dir}/snap-heckbert-float32.six"
output_png="${output_dir}/snap-heckbert-float32.png"

require_file "${input_image}"

if ! lsqa_init "$0"; then
    fail 1 "lsqa binary missing"
    exit "${status}"
fi

if ! run_img2sixel -Q heckbert -6 -W oklab \
        -o "${output_sixel}" "${input_image}" 2>>"${log_file}"; then
    fail 1 "img2sixel snap heckbert float32 failed"
    exit "${status}"
fi

if lsqa_run_benchmark "${input_image}" "${output_sixel}" \
        "snap-heckbert-float32" "${artifact_dir}" "${lsqa_floor}"; then
    pass 1 "snap heckbert float32 lsqa passed"
else
    fail 1 "snap heckbert float32 lsqa failed"
fi

exit "${status}"
