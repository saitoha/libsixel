#!/bin/sh
# Validate downscale 80pct scaling with nearest resampling.
#
# Reference image details:
# - Source: tests/data/inputs/snake_64.png (64x64)
# - ImageMagick filter: Point
# - Resize target: 80% of the original dimensions
#
# The test compares img2sixel output against the prebuilt reference image.
# The MS-SSIM floor is tuned lower than other filters because nearest-neighbour
# scaling preserves hard edges that amplify palette quantization artifacts.
set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

config_macro_defined HAVE_IMG2SIXEL || skip_all "img2sixel is disabled in this build"

lsqa_floor=${LSQA_MS_SSIM_FLOOR:-0.90}

input_image="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"
reference_image="${TOP_SRCDIR}/tests/data/inputs/scaling/snake_64_nearest_80pct.png"
output_sixel="${ARTIFACT_LOCAL_DIR}/nearest-downscale_80pct.six"

echo "1..1"
set -v

run_img2sixel -r nearest -w 80% -o "${output_sixel}" "${input_image}" || {
    fail 1 "nearest downscale 80pct scaling failed"
    exit 0
}

lsqa_err=$(
    set +xv
    run_lsqa -b "MS-SSIM:${lsqa_floor}" "${reference_image}" "${output_sixel}" 2>&1
) || lsqa_run_status=$?

test "${lsqa_run_status:-0}" -eq 0 && {
    pass 1 "nearest downscale 80pct lsqa passed"
    exit 0
}

test "${lsqa_run_status}" -eq 5 && {
    fail 1 "${lsqa_err}"
    exit 0
}

fail 1 "nearest downscale 80pct lsqa failed"

exit 0
