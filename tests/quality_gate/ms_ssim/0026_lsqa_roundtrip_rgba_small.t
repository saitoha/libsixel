#!/bin/sh
# Confirm small RGBA roundtrip retains the MS-SSIM baseline.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

config_macro_defined HAVE_IMG2SIXEL || skip_all "img2sixel is disabled in this build"
config_macro_defined HAVE_SIXEL2PNG || skip_all "sixel2png is disabled in this build"

printf '1..1\n'
set -v

# Baseline MS-SSIM measured from the current roundtrip output.
lsqa_floor=0.9

image_path="${TOP_SRCDIR}/tests/data/inputs/formats/rgba.png"
output_sixel="${ARTIFACT_LOCAL_DIR}/rgba_roundtrip.six"
output_png="${ARTIFACT_LOCAL_DIR}/rgba_roundtrip.png"

run_img2sixel -Lbuiltin! "${image_path}" >"${output_sixel}" || {
    fail 1 "rgba roundtrip encode failed"
    exit 0
}

run_sixel2png -i "${output_sixel}" -o "${output_png}" || {
    fail 1 "rgba roundtrip decode failed"
    exit 0
}

lsqa_err=$(
    set +xv
    run_lsqa -b "MS-SSIM:${lsqa_floor}" "${image_path}" "${output_png}" 2>&1
) || lsqa_run_status=$?

lsqa_status=${lsqa_run_status-0}

test "${lsqa_status}" -ne 5 || {
    fail 1 "${lsqa_err}"
    exit 0
}

test "${lsqa_status}" -eq 0 || {
    fail 1 "rgba roundtrip ms-ssim regressed"
    exit 0
}

pass 1 "rgba roundtrip ms-ssim meets baseline"


exit 0
