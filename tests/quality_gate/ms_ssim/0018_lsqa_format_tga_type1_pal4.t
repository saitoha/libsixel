#!/bin/sh
# Verify TGA type 1 (uncompressed color-mapped) with 16-color palette.
# Reproduction commands (ImageMagick):
#   convert images/snake.png -resize 64x64\! -colors 16 -type Palette -define tga:image-origin=TopLeft tests/data/inputs/formats/snake-tga-type1-pal4.tga
#   convert images/snake.png -resize 64x64\! tests/data/inputs/formats/snake-64-reference-rgb.png
#   convert images/snake.png -resize 64x64\! -colorspace Gray tests/data/inputs/formats/snake-64-reference-gray.png
#   convert images/snake.png -resize 64x64\! -alpha set -channel A -evaluate set 100% +channel tests/data/inputs/formats/snake-64-reference-rgba.png

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

lsqa_floor=${LSQA_MS_SSIM_FLOOR:-0.96}

printf '1..1\n'
set -v

image_path="${TOP_SRCDIR}/tests/data/inputs/formats/snake-tga-type1-pal4.tga"
reference_path="${TOP_SRCDIR}/tests/data/inputs/formats/snake-64-reference-rgb.png"
output_sixel="${ARTIFACT_LOCAL_DIR}/output.six"

run_img2sixel -Lbuiltin! "${image_path}" >"${output_sixel}" || {
    fail 1 "type 1 PAL4 TGA quality below floor"
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
    fail 1 "type 1 PAL4 TGA quality below floor"
    exit 0
}

pass 1 "type 1 PAL4 TGA meets lsqa floor"


exit 0
