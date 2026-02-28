#!/bin/sh
# Run lsqa quality checks for Heckbert final merge with 8-bit palettes.
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

lsqa_floor=${LSQA_MS_SSIM_FLOOR:-0.98}

echo "1..1"
set -v

input_image="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"
output_sixel="${ARTIFACT_LOCAL_DIR}/merge-heckbert-8bit.six"

SIXEL_PALETTE_OVERSPLIT_FACTOR=2.2 \
        SIXEL_PALETTE_FINAL_MERGE_ADDITIONAL_LLOYD_ITER_COUNT=2 \
        SIXEL_PALETTE_KMEANS_ITER_COUNT_MAX=5 \
        SIXEL_PALETTE_KMEANS_THRESHOLD=0.1 \
        SIXEL_PALETTE_LUMIN_FACTOR_R=0.3 \
        SIXEL_PALETTE_LUMIN_FACTOR_G=0.4 \
        SIXEL_PALETTE_MERGE_CHANNEL_FACTOR_L=0.6 \
        run_img2sixel -Q heckbert -F ward \
    -o "${output_sixel}" "${input_image}" || {
    echo "not ok" 1 "img2sixel merge heckbert 8bit failed"
    exit 0
}

lsqa_err=$(
    set +xv
    run_lsqa -b "MS-SSIM:${lsqa_floor}" "${input_image}" "${output_sixel}" 2>&1
) || lsqa_run_status=$?

test "${lsqa_run_status:-0}" -eq 0 && {
    echo "ok" 1 "merge heckbert 8bit lsqa passed"
    exit 0
}

test "${lsqa_run_status}" -eq 5 && {
    echo "not ok" 1 "${lsqa_err}"
    exit 0
}

echo "not ok" 1 "merge heckbert 8bit lsqa failed"

exit 0
