#!/bin/sh
# Ensure grayscale JPEG quality stays within the recorded lsqa baseline.
# Reproduction commands (ImageMagick):
#   convert images/snake.png -resize 64x64\! -colorspace Gray tests/data/inputs/formats/snake-64-grayscale.jpg
#   convert images/snake.png -resize 64x64\! -colorspace Gray tests/data/inputs/formats/snake-64-reference-gray.png

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

lsqa_floor=${LSQA_MS_SSIM_FLOOR:-0.98}

ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"

printf '1..1\n'
set -v

image_path="${top_srcdir}/tests/data/inputs/formats/snake-64-grayscale.jpg"
reference_path="${top_srcdir}/tests/data/inputs/formats/snake-64-reference-gray.png"
output_sixel="${ARTIFACT_LOCAL_DIR}/grayscale.six"
if run_img2sixel -Lbuiltin! "${image_path}" >"${output_sixel}"; then
    :
else
    fail 1 "grayscale quality regressed"
    exit 0
fi

lsqa_err=$(
    set +xv
    run_lsqa -b "MS-SSIM:${lsqa_floor}" "${reference_path}" "${output_sixel}" 2>&1
) || lsqa_run_status=$?

if [ -z "${lsqa_run_status-}" ]; then
    pass 1 "grayscale quality meets baseline"
elif [ "${lsqa_run_status}" -eq 5 ]; then
    fail 1 "${lsqa_err}"
else
    fail 1 "grayscale quality regressed"
fi

exit 0
