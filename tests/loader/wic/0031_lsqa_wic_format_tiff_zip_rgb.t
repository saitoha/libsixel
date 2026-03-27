#!/bin/sh
# Verify WIC TIFF ZIP RGB decoding quality with an MS-SSIM baseline.
# Reproduction command (ImageMagick):
#   convert tests/data/inputs/snake_64.png -compress Zip tests/data/inputs/formats/snake-tiff-zip-rgb.tiff

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}
test "${HAVE_WIC-}" = 1 || {
    printf "1..0 # SKIP wic loader is unavailable\n";
    exit 0
}
test "${RUNTIME_ENV_IS_WINE-0}" -eq 1 && {
    printf "1..0 # SKIP WIC is unavailable under wine\n";
    exit 0
}


printf '1..1\n'
set -v
mkdir -p "${ARTIFACT_LOCAL_DIR}"

lsqa_floor=0.99

image_path="${TOP_SRCDIR}/tests/data/inputs/formats/snake-tiff-zip-rgb.tiff"
reference_path="${TOP_SRCDIR}/tests/data/inputs/formats/snake-64-reference-rgb.ppm"
output_sixel="${ARTIFACT_LOCAL_DIR}/wic_tiff_zip_rgb.six"
${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -Lwic! "${image_path}" >"${output_sixel}" || {
    echo "not ok" 1 - "wic tiff zip rgb conversion failed"
    exit 0
}

lsqa_err=$(
    set +xv
    ${SIXEL_RUNTIME-} "${LSQA_PATH}" -b "MS-SSIM:${lsqa_floor}" "${reference_path}" "${output_sixel}" 2>&1
) || lsqa_run_status=$?

test "${lsqa_run_status:-0}" -eq 0 && {
    echo "ok" 1 - "wic tiff zip rgb quality meets baseline"
    exit 0
}

test "${lsqa_run_status}" -eq 5 && {
    echo "not ok" 1 - "${lsqa_err}"
    exit 0
}

echo "not ok" 1 - "wic tiff zip rgb quality regressed"

exit 0
