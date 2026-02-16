#!/bin/sh
# Verify palette PNG quality respects the lsqa baseline and relaxed floor.

set -eu

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

lsqa_floor=${LSQA_MS_SSIM_FLOOR:-0.99}

ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"

printf '1..1
'
set -v

image_path="${TOP_SRCDIR}/tests/data/inputs/formats/palette.png"
output_sixel="${ARTIFACT_LOCAL_DIR}/palette.six"
run_img2sixel -Lbuiltin! "${image_path}" >"${output_sixel}" || {
    fail 1 "palette quality regressed"
    exit 0
}

lsqa_err=$(
    set +xv
    run_lsqa -b "MS-SSIM:${lsqa_floor}" "${image_path}" "${output_sixel}" 2>&1
) || lsqa_run_status=$?

lsqa_status=${lsqa_run_status-0}

test "${lsqa_status}" -ne 5 || {
    fail 1 "${lsqa_err}"
    exit 0
}

test "${lsqa_status}" -eq 0 || {
    fail 1 "palette quality regressed"
    exit 0
}

pass 1 "palette quality meets baseline"
exit 0
