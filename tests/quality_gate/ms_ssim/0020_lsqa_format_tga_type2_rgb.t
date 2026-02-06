#!/bin/sh
# Verify TGA type 2 (uncompressed RGB) quality against lsqa baselines.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"
. "${TOP_SRCDIR}/tests/lib/sh/common/tap.sh"

lsqa_floor=${LSQA_MS_SSIM_FLOOR:-0.98}

ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"

printf '1..1\n'
set -v

image_path="${top_srcdir}/tests/data/inputs/formats/snake-tga-type2-rgb.tga"
output_sixel="${ARTIFACT_LOCAL_DIR}/output.six"

if ! run_img2sixel -Lbuiltin! "${image_path}" >"${output_sixel}"; then
    fail 1 "type 2 RGB TGA quality below floor"
    exit 0
fi

lsqa_err=$(
    set +xv
    run_lsqa -b "MS-SSIM:${lsqa_floor}" "${image_path}" "${output_sixel}" 2>&1
) || lsqa_run_status=$?

if [ -z "${lsqa_run_status-}" ]; then
    pass 1 "type 2 RGB TGA meets lsqa floor"
elif [ "${lsqa_run_status}" -eq 5 ]; then
    fail 1 "${lsqa_err}"
else
    fail 1 "type 2 RGB TGA quality below floor"
fi

exit 0
