#!/bin/sh
# Validate height scaling with the default bilinear resampler.
#
# Reference image details:
# - Source: tests/data/inputs/snake_64.png (64x64)
# - ImageMagick filter: Triangle (bilinear)
# - Resize target: 120% height
#
# The test compares img2sixel output against the prebuilt reference image
# using LSQA with the default MS-SSIM floor (0.98 unless overridden).
set -eux

conversion_common_path="${TOP_SRCDIR}/tests/lib/sh/conversion/common.sh"
. "${conversion_common_path}"

lsqa_common_path="${TOP_SRCDIR}/tests/lib/sh/lsqa/lsqa_common.sh"
LSQA_HELPER_DIR=$(CDPATH=; cd "$(dirname "${lsqa_common_path}")" && pwd)
export LSQA_HELPER_DIR
. "${lsqa_common_path}"

setup_conversion_env "${test_name}"

status=0
lsqa_floor=${LSQA_MS_SSIM_FLOOR:-0.98}

data_root="${top_srcdir}/tests/data/inputs"
LSQA_DATA_ROOT="${top_srcdir}/tests/data"
export LSQA_DATA_ROOT
TOP_BUILDDIR="${top_builddir}"
TOP_SRCDIR="${top_srcdir}"
export TOP_BUILDDIR TOP_SRCDIR
input_image="${data_root}/snake_64.png"
reference_image="${data_root}/scaling/snake_64_h120pct.png"
output_sixel="${ARTIFACT_LOCAL_DIR}/height_120pct.six"

ensure_img2sixel_available

echo "1..1"
set -v

require_file "${input_image}"
require_file "${reference_image}"


if run_img2sixel -h 120% -o "${output_sixel}"         "${input_image}" \
; then
    :
else
    fail 1 "height scaling with -h 120% failed"
    exit "${status}"
fi

lsqa_err=$(
    set +xv
    run_lsqa -b "MS-SSIM:${lsqa_floor}" "${reference_image}" "${output_sixel}" 2>&1
) || lsqa_run_status=$?

if [ -z "${lsqa_run_status-}" ]; then
    pass 1 "height scaling -h 120% lsqa passed"
elif [ "${lsqa_run_status}" -eq 5 ]; then
    fail 1 "${lsqa_err}"
else
    fail 1 "height scaling -h 120% lsqa failed"
fi

exit "${status}"
