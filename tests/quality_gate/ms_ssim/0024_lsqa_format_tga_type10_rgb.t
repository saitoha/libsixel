#!/bin/sh
# Verify TGA type 10 (RLE RGB) quality against lsqa baselines.
# Reproduction commands (ImageMagick):
#   convert images/snake.png -resize 64x64\! -compress RLE tests/data/inputs/formats/snake-tga-type10-rgb.tga
#   convert images/snake.png -resize 64x64\! tests/data/inputs/formats/snake-64-reference-rgb.png
#   convert images/snake.png -resize 64x64\! -colorspace Gray tests/data/inputs/formats/snake-64-reference-gray.png
#   convert images/snake.png -resize 64x64\! -alpha set -channel A -evaluate set 100% +channel tests/data/inputs/formats/snake-64-reference-rgba.png

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"

lsqa_floor=${LSQA_MS_SSIM_FLOOR:-0.18}

printf '1..1\n'
set -v

image_path="${top_srcdir}/tests/data/inputs/formats/snake-tga-type10-rgb.tga"
reference_path="${top_srcdir}/tests/data/inputs/formats/snake-64-reference-rgb.png"
output_sixel="${ARTIFACT_LOCAL_DIR}/output.six"
run_img2sixel -Lbuiltin! "${image_path}" >"${output_sixel}" || {
    fail 1 "type 10 RGB TGA quality below floor"
    exit 0
}

lsqa_err=$(
    set +xv
    run_lsqa -b "MS-SSIM:${lsqa_floor}" "${reference_path}" "${output_sixel}" 2>&1
) || lsqa_run_status=$?

if [ -z "${lsqa_run_status-}" ]; then
    pass 1 "type 10 RGB TGA meets lsqa floor"
elif [ "${lsqa_run_status}" -eq 5 ]; then
    fail 1 "${lsqa_err}"
else
    fail 1 "type 10 RGB TGA quality below floor"
fi

exit 0
