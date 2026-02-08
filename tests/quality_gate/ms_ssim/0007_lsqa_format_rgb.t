#!/bin/sh
# Confirm RGB PNG quality meets the lsqa baseline thresholds.
# Reproduction commands (ImageMagick):
#   convert images/snake.png -resize 64x64\! tests/data/inputs/snake_64.png
#   convert images/snake.png -resize 64x64\! tests/data/inputs/formats/snake-64-reference-rgb.png

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"

lsqa_floor=${LSQA_MS_SSIM_FLOOR:-0.98}

printf '1..1\n'
set -v

image_path="${top_srcdir}/tests/data/inputs/snake_64.png"
reference_path="${top_srcdir}/tests/data/inputs/formats/snake-64-reference-rgb.png"
output_sixel="${ARTIFACT_LOCAL_DIR}/rgb.six"
run_img2sixel -Lbuiltin! "${image_path}" >"${output_sixel}" || {
    fail 1 "rgb quality regressed"
    exit 0
}

lsqa_err=$(
    set +xv
    run_lsqa -b "MS-SSIM:${lsqa_floor}" "${reference_path}" "${output_sixel}" 2>&1
) || lsqa_run_status=$?

if [ -z "${lsqa_run_status-}" ]; then
    pass 1 "rgb quality meets baseline"
elif [ "${lsqa_run_status}" -eq 5 ]; then
    fail 1 "${lsqa_err}"
else
    fail 1 "rgb quality regressed"
fi

exit 0
