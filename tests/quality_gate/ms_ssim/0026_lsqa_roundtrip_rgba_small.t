#!/bin/sh
# Confirm small RGBA roundtrip retains the MS-SSIM baseline.

set -eu

if [ "${VERBOSE:-0}" -eq 1 ]; then
    set -x
fi

lsqa_common_path="${TOP_SRCDIR}/tests/lib/sh/lsqa/lsqa_common.sh"
. "${lsqa_common_path}"

status=0

# Baseline MS-SSIM measured from the current roundtrip output.
lsqa_floor=0.9

ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"
ensure_converter_available "SIXEL2PNG" "${SIXEL2PNG_PATH}" "sixel2png"
ensure_executable "${LSQA_PATH}" "lsqa"



printf '1..1\n'
set -v

image_path="${top_srcdir}/tests/data/inputs/formats/rgba.png"
output_sixel="${ARTIFACT_LOCAL_DIR}/rgba_roundtrip.six"
output_png="${ARTIFACT_LOCAL_DIR}/rgba_roundtrip.png"


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
