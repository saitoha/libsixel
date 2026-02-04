#!/bin/sh
# Confirm small RGBA roundtrip retains the MS-SSIM baseline.

set -eu

if [ "${VERBOSE:-0}" -eq 1 ]; then
    set -x
fi

test_name=$(basename "$0")
test_dir=$(CDPATH=; cd "$(dirname "$0")" && pwd)
category_name=$(basename "$(dirname "${test_dir}")")


lsqa_common_path="${TOP_SRCDIR}/tests/lib/sh/lsqa/lsqa_common.sh"
LSQA_HELPER_DIR=$(CDPATH=; cd "$(dirname "${lsqa_common_path}")" && pwd)
export LSQA_HELPER_DIR
. "${lsqa_common_path}"


status=0

# Baseline MS-SSIM measured from the current roundtrip output.
lsqa_floor=0.9

ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"
ensure_converter_available "SIXEL2PNG" "${SIXEL2PNG_PATH}" "sixel2png"
ensure_executable "${LSQA_PATH}" "lsqa"


artifact_root=${ARTIFACT_ROOT:-"$(pwd)/_artifacts"}
artifact_test_dir=$(dirname "$0")
artifact_dir="${artifact_root}/${artifact_test_dir}/${test_name}"
mkdir -p "${artifact_dir}"

printf '1..1\n'
set -v

image_path="${top_srcdir}/tests/data/inputs/formats/rgba.png"
output_sixel="${artifact_dir}/rgba_roundtrip.six"
output_png="${artifact_dir}/rgba_roundtrip.png"
require_file "${image_path}"

if run_img2sixel -Lbuiltin! "${image_path}" >"${output_sixel}"; then
    :
else
    fail 1 "rgba roundtrip encode failed"
    exit "${status}"
fi

if run_sixel2png -i "${output_sixel}" -o "${output_png}"; then
    :
else
    fail 1 "rgba roundtrip decode failed"
    exit "${status}"
fi

lsqa_err=$(
    set +xv
    run_lsqa -b "MS-SSIM:${lsqa_floor}" "${image_path}" "${output_png}" 2>&1
) || lsqa_run_status=$?

if [ -z "${lsqa_run_status-}" ]; then
    pass 1 "rgba roundtrip ms-ssim meets baseline"
elif [ "${lsqa_run_status}" -eq 5 ]; then
    fail 1 "${lsqa_err}"
else
    fail 1 "rgba roundtrip ms-ssim regressed"
fi

exit "${status}"
