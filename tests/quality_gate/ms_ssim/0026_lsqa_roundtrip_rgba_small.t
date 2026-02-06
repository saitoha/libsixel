#!/bin/sh
# Confirm small RGBA roundtrip retains the MS-SSIM baseline.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"
. "${TOP_SRCDIR}/tests/lib/sh/lsqa/lsqa_common.sh"

status=0

# Baseline MS-SSIM measured from the current roundtrip output.
lsqa_floor=0.9

config_macro_defined HAVE_IMG2SIXEL || skip_all "img2sixel is disabled in this build"
config_macro_defined HAVE_SIXEL2PNG || skip_all "sixel2png is disabled in this build"
ensure_executable "${LSQA_PATH}" "lsqa"

printf '1..1\n'
set -v

image_path="${top_srcdir}/tests/data/inputs/formats/rgba.png"
output_sixel="${ARTIFACT_LOCAL_DIR}/rgba_roundtrip.six"
output_png="${ARTIFACT_LOCAL_DIR}/rgba_roundtrip.png"

run_img2sixel -Lbuiltin! "${image_path}" >"${output_sixel}" || {
    fail 1 "rgba roundtrip encode failed"
    exit "${status}"
}

run_sixel2png -i "${output_sixel}" -o "${output_png}" || {
    fail 1 "rgba roundtrip decode failed"
    exit "${status}"
}

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
