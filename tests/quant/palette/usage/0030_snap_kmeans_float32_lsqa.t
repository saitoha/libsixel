#!/bin/sh
# Run lsqa quality checks for k-means palette snapping with float32 palettes.
set -eux

conversion_common_path="${TOP_SRCDIR}/tests/lib/sh/conversion/common.sh"
. "${conversion_common_path}"

lsqa_common_path="${TOP_SRCDIR}/tests/lib/sh/lsqa/lsqa_common.sh"
. "${lsqa_common_path}"

setup_conversion_env "${test_name}"

status=0

lsqa_floor=${LSQA_MS_SSIM_FLOOR:-0.98}

ensure_img2sixel_available

echo "1..1"
set -v

input_image="${top_srcdir}/tests/data/inputs/snake_64.png"
output_sixel="${ARTIFACT_LOCAL_DIR}/snap-kmeans-float32.six"
output_png="${output_dir}/snap-kmeans-float32.png"

require_file "${input_image}"


if SIXEL_PALETTE_SNAP_TARGET_POLICY=nearest \
        SIXEL_PALETTE_SNAP_TIMING_POLICY=all \
        SIXEL_PALETTE_SNAP_APPROACH_RATE=0.7 \
        SIXEL_PALETTE_SNAP_CHANNEL_FACTOR_L=0.7 \
        run_img2sixel -Q kmeans -6 -W oklab \
    -o "${output_sixel}" "${input_image}"; then
    :
else
    fail 1 "img2sixel snap kmeans float32 failed"
    exit "${status}"
fi

lsqa_err=$(
    set +xv
    run_lsqa -b "MS-SSIM:${lsqa_floor}" "${input_image}" "${output_sixel}" 2>&1
) || lsqa_run_status=$?

if [ -z "${lsqa_run_status-}" ]; then
    pass 1 "snap kmeans float32 lsqa passed"
elif [ "${lsqa_run_status}" -eq 5 ]; then
    fail 1 "${lsqa_err}"
else
    fail 1 "snap kmeans float32 lsqa failed"
fi

exit "${status}"
