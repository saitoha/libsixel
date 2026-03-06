#!/bin/sh
# Run lsqa quality checks for k-means final merge with float32 palettes.
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"
echo "1..1"
set -v
mkdir -p "${ARTIFACT_LOCAL_DIR}"

lsqa_floor=${LSQA_MS_SSIM_FLOOR:-0.98}


input_image="${TOP_SRCDIR}/tests/data/resolutions/tiny_square.png"
output_sixel="${ARTIFACT_LOCAL_DIR}/output.six"

run_img2sixel --env SIXEL_PALETTE_OVERSPLIT_FACTOR=1.2 \
              --env SIXEL_PALETTE_FINAL_MERGE_ADDITIONAL_LLOYD_ITER_COUNT=2 \
              --env SIXEL_PALETTE_KMEANS_ITER_COUNT_MAX=3 \
              --env SIXEL_PALETTE_KMEANS_THRESHOLD=0.1 \
              --env SIXEL_PALETTE_LUMIN_FACTOR_R=0.3 \
              --env SIXEL_PALETTE_LUMIN_FACTOR_G=0.4 \
              --env SIXEL_PALETTE_MERGE_CHANNEL_FACTOR_L=0.6 \
              -Q kmeans -F ward -W oklab -o "${output_sixel}" "${input_image}" || {
    echo "not ok" 1 - "img2sixel merge kmeans float32 failed"
    exit 0
}

lsqa_err=$(
    set +xv
    run_lsqa -b "MS-SSIM:${lsqa_floor}" "${input_image}" "${output_sixel}" 2>&1
) || lsqa_run_status=$?

test "${lsqa_run_status-}" = 5 && {
    echo "not ok" 1 - "${lsqa_err}"
    exit 0
}

test -n "${lsqa_run_status-}" && {
    echo "not ok" 1 - "merge kmeans float32 lsqa failed (${lsqa_run_status-})"
    exit 0
}

echo "ok" 1 - "merge kmeans float32 lsqa passed"
exit 0
