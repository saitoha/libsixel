#!/bin/sh
# Run lsqa checks for float32 Eytzinger in the gamma colorspace.
# Quality floors tuned to 99% of the current lsqa MS-SSIM metric:
# - MS-SSIM floor: 0.976111
set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"
. "${TOP_SRCDIR}/tests/lib/sh/common/tap.sh"
. "${TOP_SRCDIR}/tests/lib/sh/lsqa/lsqa_common.sh"

status=0

lsqa_floor=${LSQA_MS_SSIM_FLOOR:-0.98}

config_macro_defined HAVE_IMG2SIXEL || skip_all "img2sixel is disabled in this build"

echo "1..1"
set -v

input_image="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"
output_sixel="${ARTIFACT_LOCAL_DIR}/eytzinger-float32-gamma.six"
output_png="${ARTIFACT_LOCAL_DIR}/eytzinger-float32-gamma.png"

if run_img2sixel --lookup-policy=eytzinger --precision=float32 \
        --working-colorspace=gamma -o "${output_sixel}" "${input_image}"; then
    :
else
    fail 1 "float32 Eytzinger gamma colorspace conversion failed"
    exit "${status}"
fi

lsqa_err=$(
    set +xv
    run_lsqa -b "MS-SSIM:${lsqa_floor}" "${input_image}" "${output_sixel}" 2>&1
) || lsqa_run_status=$?

if [ -z "${lsqa_run_status-}" ]; then
    pass 1 "float32 Eytzinger gamma colorspace lsqa passed"
elif [ "${lsqa_run_status}" -eq 5 ]; then
    fail 1 "${lsqa_err}"
else
    fail 1 "float32 Eytzinger gamma colorspace lsqa failed"
fi

exit "${status}"
