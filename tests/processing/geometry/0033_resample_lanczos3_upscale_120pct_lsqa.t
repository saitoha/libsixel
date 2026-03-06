#!/bin/sh
# Validate upscale 120pct scaling with lanczos3 resampling.
#
# Reference image details:
# - Source: tests/data/inputs/snake_64.png (64x64)
# - ImageMagick filter: Lanczos (lobes=3)
# - Resize target: 120% of the original dimensions
#
# The test compares img2sixel output against the prebuilt reference image
# using LSQA with the default MS-SSIM floor (0.98 unless overridden).
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

lsqa_floor=${LSQA_MS_SSIM_FLOOR:-0.98}

data_root="${TOP_SRCDIR}/tests/data/inputs"
input_image="${data_root}/snake_64.png"
reference_image="${data_root}/scaling/snake_64_lanczos3_120pct.png"
output_sixel="${ARTIFACT_LOCAL_DIR}/lanczos3-upscale_120pct.six"

echo "1..1"
set -v

run_img2sixel -r lanczos3 -w 120% -o "${output_sixel}" "${input_image}" || {
    echo "not ok" 1 - "lanczos3 upscale 120pct scaling failed"
    exit 0
}

lsqa_err=$(
    set +xv
    run_lsqa -b "MS-SSIM:${lsqa_floor}" "${reference_image}" "${output_sixel}" 2>&1
) || lsqa_run_status=$?

test "${lsqa_run_status:-0}" -eq 0 && {
    echo "ok" 1 - "lanczos3 upscale 120pct lsqa passed"
    exit 0
}

test "${lsqa_run_status}" -eq 5 && {
    echo "not ok" 1 - "${lsqa_err}"
    exit 0
}

echo "not ok" 1 - "lanczos3 upscale 120pct lsqa failed"

exit 0
