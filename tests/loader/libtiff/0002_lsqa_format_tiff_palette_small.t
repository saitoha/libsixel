#!/bin/sh
# Confirm small palette TIFF quality meets the MS-SSIM baseline.
# Reproduction commands (ImageMagick):
#   convert images/snake.png -resize 64x64\! -colors 256 -type Palette tests/data/inputs/snake_64-indexed.tiff
#   convert images/snake.png -resize 64x64\! tests/data/inputs/formats/snake-64-reference-rgb.ppm
#   convert images/snake.png -resize 64x64\! -colorspace Gray tests/data/inputs/formats/snake-64-reference-gray.ppm
#   convert images/snake.png -resize 64x64\! -alpha set -channel A -evaluate set 100% +channel tests/data/inputs/formats/snake-64-reference-rgba.ppm

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}
test "${HAVE_LIBTIFF-}" = 1 || {
    printf "1..0 # SKIP libtiff support is disabled in this build\n"
    exit 0
}


printf '1..1\n'
set -v
mkdir -p "${ARTIFACT_LOCAL_DIR}"

lsqa_floor=0.98

image_path="${TOP_SRCDIR}/tests/data/inputs/snake_64-indexed.tiff"
reference_path="${TOP_SRCDIR}/tests/data/inputs/formats/snake-64-reference-rgb.ppm"
output_sixel="${ARTIFACT_LOCAL_DIR}/snake_64_indexed.six"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -Llibtiff! "${image_path}" >"${output_sixel}" || {
    echo "not ok" 1 - "tiff palette conversion failed"
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
    echo "not ok" 1 - "tiff palette quality regressed"
    exit 0
}

echo "ok" 1 - "tiff palette quality meets baseline"
exit 0
