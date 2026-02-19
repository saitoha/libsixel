#!/bin/sh
# Verify WIC JPEG progressive 4:2:0 decoding quality with an MS-SSIM baseline.
# Reproduction command (ImageMagick):
#   convert tests/data/inputs/snake_64.png -sampling-factor 4:2:0 -interlace Plane tests/data/inputs/formats/snake-jpeg-progressive-420.jpg

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

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

printf '1..1
'
set -v

lsqa_floor=0.95

image_path="${TOP_SRCDIR}/tests/data/inputs/formats/snake-jpeg-progressive-420.jpg"
reference_path="${TOP_SRCDIR}/tests/data/inputs/formats/snake-64-reference-rgb.png"
output_sixel="${ARTIFACT_LOCAL_DIR}/wic_jpeg_progressive_420.six"
run_img2sixel -Lwic! "${image_path}" >"${output_sixel}" || {
    fail 1 "wic jpeg progressive 420 conversion failed"
    exit 0
}

lsqa_err=$(
    run_lsqa -b "MS-SSIM:${lsqa_floor}" "${reference_path}" "${output_sixel}" 2>&1
) || lsqa_run_status=$?

test "${lsqa_run_status:-0}" -eq 0 && {
    pass 1 "wic jpeg progressive 420 quality meets baseline"
    exit 0
}

test "${lsqa_run_status}" -eq 5 && {
    fail 1 "${lsqa_err}"
    exit 0
}

fail 1 "wic jpeg progressive 420 quality regressed"

exit 0
