#!/bin/sh
# Run lsqa quality checks for k-means final merge with 8-bit palettes.
set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

lsqa_floor=${LSQA_MS_SSIM_FLOOR:-0.98}

config_macro_defined HAVE_IMG2SIXEL || skip_all "img2sixel is disabled in this build"

echo "1..1"
set -v

input_image="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"
output_sixel="${ARTIFACT_LOCAL_DIR}/merge-kmeans-8bit.six"
output_png="${ARTIFACT_LOCAL_DIR}/merge-kmeans-8bit.png"

SIXEL_PALETTE_OVERSPLIT_FACTOR=2.2 \
        SIXEL_PALETTE_FINAL_MERGE_ADDITIONAL_LLOYD_ITER_COUNT=2 \
        SIXEL_PALETTE_KMEANS_ITER_COUNT_MAX=5 \
        SIXEL_PALETTE_KMEANS_THRESHOLD=0.1 \
        SIXEL_PALETTE_LUMIN_FACTOR_R=0.3 \
        SIXEL_PALETTE_LUMIN_FACTOR_G=0.4 \
        SIXEL_PALETTE_MERGE_CHANNEL_FACTOR_L=0.6 \
        run_img2sixel -Q kmeans -F ward \
    -o "${output_sixel}" "${input_image}" || {
    fail 1 "img2sixel merge kmeans 8bit failed"
    exit 0
}

lsqa_err=$(
    set +xv
    run_lsqa -b "MS-SSIM:${lsqa_floor}" "${input_image}" "${output_sixel}" 2>&1
) || lsqa_run_status=$?

if [ -z "${lsqa_run_status-}" ]; then
    pass 1 "merge kmeans 8bit lsqa passed"
elif [ "${lsqa_run_status}" -eq 5 ]; then
    fail 1 "${lsqa_err}"
else
    fail 1 "merge kmeans 8bit lsqa failed"
fi

exit 0
