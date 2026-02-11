#!/bin/sh
# Verify TGA type 9 (RLE color-mapped) with 256-color palette.
# Reproduction commands (ImageMagick):
#   convert images/snake.png -resize 64x64\! -colors 256 -type Palette -compress RLE tests/data/inputs/formats/snake-tga-type9-pal8.tga
#   convert images/snake.png -resize 64x64\! tests/data/inputs/formats/snake-64-reference-rgb.png
#   convert tests/data/inputs/formats/snake-64-reference-rgb.png -flip tests/data/inputs/formats/snake-64-reference-rgb-flip.png
#   convert images/snake.png -resize 64x64\! -colorspace Gray tests/data/inputs/formats/snake-64-reference-gray.png
#   convert tests/data/inputs/formats/snake-64-reference-gray.png -flip tests/data/inputs/formats/snake-64-reference-gray-flip.png
#   convert images/snake.png -resize 64x64\! -alpha set -channel A -evaluate set 100% +channel tests/data/inputs/formats/snake-64-reference-rgba.png

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

lsqa_floor=${LSQA_MS_SSIM_FLOOR:-0.9970}

ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"

printf '1..1\n'
set -v

image_path="${top_srcdir}/tests/data/inputs/formats/snake-tga-type9-pal8.tga"
reference_path="${top_srcdir}/tests/data/inputs/formats/snake-64-reference-rgb-flip.png"
output_sixel="${ARTIFACT_LOCAL_DIR}/output.six"
run_img2sixel -Lbuiltin! "${image_path}" >"${output_sixel}" || {
    fail 1 "type 9 PAL8 TGA quality below floor"
    exit 0
}

lsqa_err=$(
    set +xv
    run_lsqa -b "MS-SSIM:${lsqa_floor}" "${reference_path}" "${output_sixel}" 2>&1
) || lsqa_run_status=$?

lsqa_status=${lsqa_run_status-0}

test "${lsqa_status}" -ne 5 || {
    fail 1 "${lsqa_err}"
    exit 0
}

test "${lsqa_status}" -eq 0 || {
    fail 1 "type 9 PAL8 TGA quality below floor"
    exit 0
}

pass 1 "type 9 PAL8 TGA meets lsqa floor"


exit 0
