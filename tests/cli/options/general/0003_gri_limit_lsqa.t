#!/bin/sh
# TAP test validating --gri-limit preserves output quality and determinism.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

config_macro_defined HAVE_IMG2SIXEL || skip_all "img2sixel is disabled in this build"

lsqa_floor=1.0
lsqa_run_status=0

echo "1..1"
set -v

input_image="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"
output_plain="${ARTIFACT_LOCAL_DIR}/plain.six"
output_limited="${ARTIFACT_LOCAL_DIR}/limited.six"
lsqa_err_file="${ARTIFACT_LOCAL_DIR}/gri-limit-lsqa.err"

run_img2sixel -=1 -o "${output_plain}" "${input_image}" || {
    fail 1 "img2sixel failed"
    exit 0
}
run_img2sixel -=1 --gri-limit -o "${output_limited}" "${input_image}" || {
    fail 1 "img2sixel failed"
    exit 0
}

: >"${lsqa_err_file}"
run_lsqa -b "MS-SSIM:${lsqa_floor}" "${output_plain}" "${output_limited}"     > /dev/null 2>"${lsqa_err_file}" || lsqa_run_status=$?

test "${lsqa_run_status}" -eq 0 && {
    pass 1 "gri-limit deterministic output matches"
    exit 0
}

test "${lsqa_run_status}" -eq 5 && {
    fail 1 "$(cat "${lsqa_err_file}")"
    exit 0
}

fail 1 "gri-limit deterministic output mismatch"
exit 0
