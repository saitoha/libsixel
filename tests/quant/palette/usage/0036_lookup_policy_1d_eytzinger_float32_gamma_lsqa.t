#!/bin/sh
# Run lsqa checks for float32 Eytzinger in the gamma colorspace.
# Quality floors tuned to 99% of the current lsqa MS-SSIM metric:
# - MS-SSIM floor: 0.976111
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

ensure_img2sixel_available

echo "1..1"
set -v

input_image="${top_srcdir}/tests/data/inputs/snake_64.png"
output_sixel="${ARTIFACT_LOCAL_DIR}/eytzinger-float32-gamma.six"
output_png="${output_dir}/eytzinger-float32-gamma.png"

require_file "${input_image}"


if run_img2sixel --lookup-policy=eytzinger --precision=float32 \
        --working-colorspace=gamma \
    -o "${output_sixel}" "${input_image}"; then
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
