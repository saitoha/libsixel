#!/bin/sh
# TAP test covering fixed float32 Sierra-3 diffusion.
#
# Flow:
# - Convert the 64x64 snake reference with the target dithering options.
# - Decode the sixel output back to PNG for inspection.
# - Run lsqa to ensure quality stays above the minimum floors.

set -eux

conversion_common_path=$(CDPATH=; cd "$(dirname "$0")/.." && pwd)/../lib/sh/conversion/common.sh
dither_lsqa_path=$(CDPATH=; cd "$(dirname "$0")/.." && pwd)/../lib/sh/lsqa/lsqa_dither_common.sh
DITHER_LSQA_HELPER_DIR=$(CDPATH=; cd "$(dirname "${dither_lsqa_path}")" && pwd)
. "${conversion_common_path}"
. "${dither_lsqa_path}"

test_name=$(basename "$0")
setup_conversion_env "${test_name}"

status=0

ensure_img2sixel_available
ensure_converter_available "SIXEL2PNG" "${SIXEL2PNG_PATH}" "sixel2png"

if ! dither_lsqa_init "$0"; then
    fail 1 "lsqa binary missing"
    exit "${status}"
fi

echo "1..1"

input_image="${LSQA_INPUT_ROOT}/inputs/snake_64.png"
case_id=${test_name%.t}
output_sixel="${output_dir}/${case_id}.six"
output_png="${output_dir}/${case_id}.png"

require_file "${input_image}"

if run_img2sixel -d sierra3 -y raster -W oklab \
        -o "${output_sixel}" "${input_image}" 2>>"${log_file}" && \
        run_sixel2png -o "${output_png}" "${output_sixel}" 2>>"${log_file}" && \
        dither_lsqa_assert_quality "${input_image}" "${output_png}" \
        "${case_id}" "${artifact_dir}"; then
    pass 1 "fixed float32 Sierra-3 diffusion lsqa passed"
else
    fail 1 "fixed float32 Sierra-3 diffusion lsqa failed"
fi

exit "${status}"
