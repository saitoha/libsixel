#!/bin/sh
# Run lsqa quality checks for k-means palette snapping with 8-bit palettes.
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
output_sixel="${artifact_dir}/snap-kmeans-8bit.six"
output_png="${output_dir}/snap-kmeans-8bit.png"

require_file "${input_image}"

if ! lsqa_init "$0"; then
    fail 1 "lsqa binary missing"
    exit "${status}"
fi

if SIXEL_PALETTE_SNAP_TARGET_POLICY=nearest \
        SIXEL_PALETTE_SNAP_TIMING_POLICY=all \
        SIXEL_PALETTE_SNAP_APPROACH_RATE=0.7 \
        SIXEL_PALETTE_SNAP_CHANNEL_FACTOR_L=0.7 \
        run_img2sixel -Q kmeans -6 \
        -o "${output_sixel}" "${input_image}" 2>>"${log_file}"; then
    :
else
    fail 1 "img2sixel snap kmeans 8bit failed"
    exit "${status}"
fi

if lsqa_run_benchmark "${input_image}" "${output_sixel}" \
        "snap-kmeans-8bit" "${artifact_dir}" "${lsqa_floor}"; then
    pass 1 "snap kmeans 8bit lsqa passed"
else
    fail 1 "snap kmeans 8bit lsqa failed"
fi

exit "${status}"
