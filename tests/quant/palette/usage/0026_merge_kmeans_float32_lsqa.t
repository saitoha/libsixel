#!/bin/sh
# Run lsqa quality checks for k-means final merge with float32 palettes.
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

input_image="${top_srcdir}/tests/data/resolutions/tiny_square.png"
output_sixel="${ARTIFACT_LOCAL_DIR}/merge-kmeans-float32.six"
output_png="${output_dir}/merge-kmeans-float32.png"




SIXEL_PALETTE_OVERSPLIT_FACTOR=2.2
SIXEL_PALETTE_FINAL_MERGE_ADDITIONAL_LLOYD_ITER_COUNT=2
SIXEL_PALETTE_KMEANS_ITER_COUNT_MAX=5
SIXEL_PALETTE_KMEANS_THRESHOLD=0.1
SIXEL_PALETTE_LUMIN_FACTOR_R=0.3
SIXEL_PALETTE_LUMIN_FACTOR_G=0.4
SIXEL_PALETTE_MERGE_CHANNEL_FACTOR_L=0.6
export SIXEL_PALETTE_OVERSPLIT_FACTOR
export SIXEL_PALETTE_FINAL_MERGE_ADDITIONAL_LLOYD_ITER_COUNT
export SIXEL_PALETTE_KMEANS_ITER_COUNT_MAX
export SIXEL_PALETTE_KMEANS_THRESHOLD
export SIXEL_PALETTE_LUMIN_FACTOR_R
export SIXEL_PALETTE_LUMIN_FACTOR_G
export SIXEL_PALETTE_MERGE_CHANNEL_FACTOR_L

if run_img2sixel -Q kmeans -F ward -W oklab \
    -o "${output_sixel}" "${input_image}"; then
    :
else
    fail 1 "img2sixel merge kmeans float32 failed"
    exit "${status}"
fi

lsqa_err=$(
    set +xv
    run_lsqa -b "MS-SSIM:${lsqa_floor}" "${input_image}" "${output_sixel}" 2>&1
) || lsqa_run_status=$?

if [ -z "${lsqa_run_status-}" ]; then
    pass 1 "merge kmeans float32 lsqa passed"
elif [ "${lsqa_run_status}" -eq 5 ]; then
    fail 1 "${lsqa_err}"
else
    fail 1 "merge kmeans float32 lsqa failed"
fi

exit "${status}"
