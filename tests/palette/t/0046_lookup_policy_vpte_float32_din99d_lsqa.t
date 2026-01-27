#!/bin/sh
# Run lsqa checks for float32 VPTE in the din99d colorspace.
# The lsqa helper can read SIXEL directly, so compare with SIXEL output.
# Quality floors tuned to requested QA thresholds:
# - MS-SSIM floor: 0.97
set -eux

conversion_common_path=$(CDPATH=; cd "$(dirname "$0")/.." && pwd)/../lib/sh/conversion/common.sh
. "${conversion_common_path}"

lsqa_common_path=$(CDPATH=; cd "$(dirname "$0")/.." && pwd)/../lib/sh/lsqa/lsqa_common.sh
LSQA_HELPER_DIR=$(CDPATH=; cd "$(dirname "${lsqa_common_path}")" && pwd)
export LSQA_HELPER_DIR
. "${lsqa_common_path}"

test_name=$(basename "$0")
setup_conversion_env "${test_name}"

status=0

lsqa_floor=${LSQA_MS_SSIM_FLOOR:-0.98}

ensure_img2sixel_available
echo "1..1"
set -v

input_image="${images_dir}/snake.png"
output_sixel="${output_dir}/vpte-float32-din99d.six"

require_file "${input_image}"

if ! lsqa_init "$0"; then
    fail 1 "lsqa binary missing"
    exit "${status}"
fi

if run_img2sixel --lookup-policy=vpte --precision=float32 \
        --working-colorspace=din99d -o "${output_sixel}" \
        "${input_image}" \
        2>>"${log_file}"; then
    :
else
    fail 1 "float32 VPTE din99d colorspace conversion failed"
    exit "${status}"
fi

if lsqa_assert_quality "${input_image}" "${output_sixel}" \
        "vpte-float32-din99d" "${artifact_dir}" "${lsqa_floor}"; then
    pass 1 "float32 VPTE din99d colorspace lsqa passed"
else
    fail 1 "float32 VPTE din99d colorspace lsqa failed"
fi

exit "${status}"
