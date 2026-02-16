#!/bin/sh
# Validate height scaling with the default bilinear resampler.
#
# Reference image details:
# - Source: tests/data/inputs/snake_64.png (64x64)
# - ImageMagick filter: Triangle (bilinear)
# - Resize target: 80% height
#
# The test compares img2sixel output against the prebuilt reference image
# using LSQA with the default MS-SSIM floor (0.98 unless overridden).
set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

lsqa_floor=${LSQA_MS_SSIM_FLOOR:-0.98}

data_root="${top_srcdir}/tests/data/inputs"
input_image="${data_root}/snake_64.png"
reference_image="${data_root}/scaling/snake_64_h80pct.png"
output_sixel="${ARTIFACT_LOCAL_DIR}/height_80pct.six"

config_macro_defined HAVE_IMG2SIXEL || skip_all "img2sixel is disabled in this build"

echo "1..1"
set -v

run_img2sixel -h 80% -o "${output_sixel}" "${input_image}" || {
    fail 1 "height scaling with -h 80% failed"
    exit 0
}

lsqa_err=$(
    set +xv
    run_lsqa -b "MS-SSIM:${lsqa_floor}" "${reference_image}" "${output_sixel}" 2>&1
) || lsqa_run_status=$?

test "${lsqa_run_status:-0}" -eq 0 && {
    pass 1 "height scaling -h 80% lsqa passed"
    exit 0
}

test "${lsqa_run_status}" -eq 5 && {
    fail 1 "${lsqa_err}"
    exit 0
}

fail 1 "height scaling -h 80% lsqa failed"

exit 0
