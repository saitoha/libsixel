#!/bin/sh
# Verify TGA type 9 (RLE color-mapped) with 16-color palette.
# Reproduction commands (ImageMagick):
#   convert images/snake.png -resize 64x64\! -colors 16 -type Palette -compress RLE -define tga:image-origin=TopLeft tests/data/inputs/formats/snake-tga-type9-pal4.tga
#   convert images/snake.png -resize 64x64\! tests/data/inputs/formats/snake-64-reference-rgb.ppm
#   convert images/snake.png -resize 64x64\! -colorspace Gray tests/data/inputs/formats/snake-64-reference-gray.ppm
#   convert images/snake.png -resize 64x64\! -alpha set -channel A -evaluate set 100% +channel tests/data/inputs/formats/snake-64-reference-rgba.ppm

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

printf '1..1\n'
set -v
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

lsqa_floor=${LSQA_MS_SSIM_FLOOR:-0.96}


image_path="${TOP_SRCDIR}/tests/data/inputs/formats/snake-tga-type9-pal4.tga"
reference_path="${TOP_SRCDIR}/tests/data/inputs/formats/snake-64-reference-rgb.ppm"
output_sixel="${ARTIFACT_LOCAL_DIR}/output.six"
${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -Lbuiltin! "${image_path}" >"${output_sixel}" || {
    echo "not ok" 1 - "type 9 PAL4 TGA quality below floor"
    exit 0
}

lsqa_err=$(
    set +xv
    ${SIXEL_RUNTIME-} "${LSQA_PATH}" -b "MS-SSIM:${lsqa_floor}" "${reference_path}" "${output_sixel}" 2>&1
) || lsqa_run_status=$?

lsqa_status=${lsqa_run_status-0}

test "${lsqa_status}" -ne 5 || {
    echo "not ok" 1 - "${lsqa_err}"
    exit 0
}

test "${lsqa_status}" -eq 0 || {
    echo "not ok" 1 - "type 9 PAL4 TGA quality below floor"
    exit 0
}

echo "ok" 1 - "type 9 PAL4 TGA meets lsqa floor"


exit 0
