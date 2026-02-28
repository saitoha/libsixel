#!/bin/sh
# Validate height scaling with the default bilinear resampler.
#
# Reference image details:
# - Source: tests/data/inputs/snake_64.png (64x64)
# - ImageMagick filter: Triangle (bilinear)
# - Resize target: 32px height (explicit pixels)
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
reference_image="${data_root}/scaling/snake_64_h32px.png"
output_sixel="${ARTIFACT_LOCAL_DIR}/height_32px.six"

echo "1..1"
set -v

run_img2sixel -h 32px -o "${output_sixel}" "${input_image}" || {
    fail 1 "height scaling with -h 32px failed"
    exit 0
}

lsqa_err=$(
    set +xv
    run_lsqa -b "MS-SSIM:${lsqa_floor}" "${reference_image}" "${output_sixel}" 2>&1
) || lsqa_run_status=$?

test "${lsqa_run_status:-0}" -eq 0 && {
    pass 1 "height scaling -h 32px lsqa passed"
    exit 0
}

test "${lsqa_run_status}" -eq 5 && {
    fail 1 "${lsqa_err}"
    exit 0
}

fail 1 "height scaling -h 32px lsqa failed"

exit 0
