#!/bin/sh
# TAP test covering positional float32 A dither in sRGB gamma space.
#
# Flow:
# - Convert the 64x64 snake reference with the target dithering options.
# - Decode the sixel output back to PNG for inspection.
# - Run lsqa to ensure quality stays above the minimum floors.

set -eux

conversion_common_path="${TOP_SRCDIR}/tests/lib/sh/conversion/common.sh"
lsqa_common_path="${TOP_SRCDIR}/tests/lib/sh/lsqa/lsqa_common.sh"
LSQA_HELPER_DIR=$(CDPATH=; cd "$(dirname "${lsqa_common_path}")" && pwd)
. "${conversion_common_path}"
. "${lsqa_common_path}"

setup_conversion_env "${test_name}"

status=0

lsqa_floor=${LSQA_MS_SSIM_FLOOR:-0.98}

ensure_img2sixel_available


echo "1..1"
set -v

input_image="${top_srcdir}/tests/data/inputs/snake_64.png"
case_id=${test_name%.t}
output_sixel="${ARTIFACT_LOCAL_DIR}/${case_id}.six"
output_png="${output_dir}/${case_id}.png"

require_file "${input_image}"

run_img2sixel -d a_dither -y raster -W gamma -o "${output_sixel}" \
    "${input_image}" || {
    fail 1 "positional float32 a_dither gamma lsqa failed"
    exit "${status}"
}

lsqa_err=$(
    set +xv
    run_lsqa -b "MS-SSIM:${lsqa_floor}" "${input_image}" \
        "${output_sixel}" 2>&1
) || lsqa_run_status=$?

if [ -z "${lsqa_run_status-}" ]; then
    pass 1 "positional float32 a_dither gamma lsqa passed"
elif [ "${lsqa_run_status}" -eq 5 ]; then
    fail 1 "${lsqa_err}"
else
    fail 1 "positional float32 a_dither gamma lsqa failed"
fi

exit "${status}"
