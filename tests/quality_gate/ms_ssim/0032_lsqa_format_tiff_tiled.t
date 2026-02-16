#!/bin/sh
# Confirm tiled TIFF quality meets the MS-SSIM baseline.
# Reproduction commands (ImageMagick):
#   convert images/snake.png -resize 64x64\! -define tiff:tile-geometry=32x32 tests/data/inputs/formats/rgb-tiled.tiff
#   convert images/snake.png -resize 64x64\! tests/data/inputs/formats/snake-64-reference-rgb.png
#   convert images/snake.png -resize 64x64\! -colorspace Gray tests/data/inputs/formats/snake-64-reference-gray.png
#   convert images/snake.png -resize 64x64\! -alpha set -channel A -evaluate set 100% +channel tests/data/inputs/formats/snake-64-reference-rgba.png

set -eu

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"

lsqa_floor=${LSQA_MS_SSIM_FLOOR:-0.98}

feature_defined_in_config "HAVE_LIBTIFF" || skip_all "libtiff support is disabled in this build"

printf '1..1\n'
set -v

image_path="${top_srcdir}/tests/data/inputs/formats/rgb-tiled.tiff"
reference_path="${top_srcdir}/tests/data/inputs/formats/snake-64-reference-rgb.png"
output_sixel="${ARTIFACT_LOCAL_DIR}/rgb_tiled.six"
run_img2sixel -Llibtiff! "${image_path}" >"${output_sixel}" || {
    fail 1 "tiff tiled conversion failed"
    exit 0
}

lsqa_err=$(
    set +xv
    run_lsqa -b "MS-SSIM:${lsqa_floor}" "${reference_path}" "${output_sixel}" 2>&1
) || lsqa_run_status=$?

test "${lsqa_run_status:-0}" -eq 0 && {
    pass 1 "tiff tiled quality meets baseline"
    exit 0
}

test "${lsqa_run_status}" -eq 5 && {
    fail 1 "${lsqa_err}"
    exit 0
}

fail 1 "tiff tiled quality regressed"

exit 0
