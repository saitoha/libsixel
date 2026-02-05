#!/bin/sh
# Validate height scaling with the default bilinear resampler.
#
# Reference image details:
# - Source: tests/data/inputs/snake_64.png (64x64)
# - ImageMagick filter: Triangle (bilinear)
# - Resize target: 32px height (implicit pixels)
#
# The test compares img2sixel output against the prebuilt reference image
# using LSQA with the default MS-SSIM floor (0.98 unless overridden).
set -eux

conversion_common_path="${TOP_SRCDIR}/tests/lib/sh/conversion/common.sh"
. "${conversion_common_path}"

lsqa_common_path="${TOP_SRCDIR}/tests/lib/sh/lsqa/lsqa_common.sh"
. "${lsqa_common_path}"

setup_conversion_env "${test_name}"

status=0
lsqa_floor=${LSQA_MS_SSIM_FLOOR:-0.98}

data_root="${top_srcdir}/tests/data/inputs"
input_image="${data_root}/snake_64.png"
reference_image="${data_root}/scaling/snake_64_h32.png"
output_sixel="${ARTIFACT_LOCAL_DIR}/height_32.six"

ensure_img2sixel_available

echo "1..1"
set -v





if run_img2sixel -h 32 -o "${output_sixel}"         "${input_image}" \
; then
    :
else
    fail 1 "height scaling with -h 32 failed"
    exit "${status}"
fi

lsqa_err=$(
    set +xv
    run_lsqa -b "MS-SSIM:${lsqa_floor}" "${reference_image}" "${output_sixel}" 2>&1
) || lsqa_run_status=$?

if [ -z "${lsqa_run_status-}" ]; then
    pass 1 "height scaling -h 32 lsqa passed"
elif [ "${lsqa_run_status}" -eq 5 ]; then
    fail 1 "${lsqa_err}"
else
    fail 1 "height scaling -h 32 lsqa failed"
fi

exit "${status}"
