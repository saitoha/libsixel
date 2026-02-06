#!/bin/sh
# Validate downscale 80pct scaling with gaussian resampling.
#
# Reference image details:
# - Source: tests/data/inputs/snake_64.png (64x64)
# - ImageMagick filter: Gaussian
# - Resize target: 80% of the original dimensions
#
# The test compares img2sixel output against the prebuilt reference image
# using LSQA with the default MS-SSIM floor (0.98 unless overridden).
set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"
. "${TOP_SRCDIR}/tests/lib/sh/common/tap.sh"
. "${TOP_SRCDIR}/tests/lib/sh/lsqa/lsqa_common.sh"

status=0
lsqa_floor=${LSQA_MS_SSIM_FLOOR:-0.98}

data_root="${top_srcdir}/tests/data/inputs"
input_image="${data_root}/snake_64.png"
reference_image="${data_root}/scaling/snake_64_gaussian_80pct.png"
output_sixel="${ARTIFACT_LOCAL_DIR}/gaussian-downscale_80pct.six"

config_macro_defined HAVE_IMG2SIXEL || skip_all

echo "1..1"
set -v

run_img2sixel -r gaussian -w 80% -o "${output_sixel}" "${input_image}" || {
    fail 1 "gaussian downscale 80pct scaling failed"
    exit "${status}"
}

lsqa_err=$(
    set +xv
    run_lsqa -b "MS-SSIM:${lsqa_floor}" "${reference_image}" "${output_sixel}" 2>&1
) || lsqa_run_status=$?

if [ -z "${lsqa_run_status-}" ]; then
    pass 1 "gaussian downscale 80pct lsqa passed"
elif [ "${lsqa_run_status}" -eq 5 ]; then
    fail 1 "${lsqa_err}"
else
    fail 1 "gaussian downscale 80pct lsqa failed"
fi

exit "${status}"
