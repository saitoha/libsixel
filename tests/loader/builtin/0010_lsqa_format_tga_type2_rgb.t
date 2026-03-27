#!/bin/sh
# Verify TGA type 2 (uncompressed RGB) quality against lsqa baselines.
# Reproduction commands (ImageMagick):
#   convert images/snake.png -resize 64x64\! -define tga:image-origin=TopLeft tests/data/inputs/formats/snake-tga-type2-rgb.tga
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
mkdir -p "${ARTIFACT_LOCAL_DIR}"

lsqa_floor=${LSQA_MS_SSIM_FLOOR:-0.98}


image_path="${TOP_SRCDIR}/tests/data/inputs/formats/snake-tga-type2-rgb.tga"
reference_path="${TOP_SRCDIR}/tests/data/inputs/formats/snake-64-reference-rgb.ppm"
output_sixel="${ARTIFACT_LOCAL_DIR}/output.six"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -Lbuiltin! "${image_path}" >"${output_sixel}" || {
    echo "not ok" 1 - "type 2 RGB TGA quality below floor"
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
    echo "not ok" 1 - "type 2 RGB TGA quality below floor"
    exit 0
}

echo "ok" 1 - "type 2 RGB TGA meets lsqa floor"


exit 0
