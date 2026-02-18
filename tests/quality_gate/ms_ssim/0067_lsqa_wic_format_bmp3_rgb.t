#!/bin/sh
# Verify WIC BMP3 RGB decoding quality with an MS-SSIM baseline.
# Reproduction command (ImageMagick):
#   convert tests/data/inputs/snake_64.png -colors 256 BMP2:tests/data/inputs/formats/snake-bmp2-pal8.bmp
#   convert tests/data/inputs/snake_64.png BMP3:tests/data/inputs/formats/snake-bmp3-rgb.bmp
#   convert tests/data/inputs/snake_64.png -interlace Plane tests/data/inputs/formats/snake-gif-interlaced.gif
#   convert tests/data/inputs/snake_64.png -colors 256 tests/data/inputs/formats/snake-ico-pal8.ico
#   convert tests/data/inputs/formats/rgba.png tests/data/inputs/formats/snake-ico-rgba.ico
#   convert tests/data/inputs/snake_64.png DXT1:tests/data/inputs/formats/snake-dds-dxt1.dds
#   convert tests/data/inputs/formats/rgba.png DXT5:tests/data/inputs/formats/snake-dds-dxt5.dds

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build";
    exit 0
}
test "${HAVE_WIC-}" = 1 || {
    printf "1..0 # SKIP wic loader is unavailable";
    exit 0
}
test "${RUNTIME_ENV_IS_WINE-0}" -eq 1 && {
    printf "1..0 # SKIP WIC is unavailable under wine";
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

printf '1..1\n'
set -v

lsqa_floor=0.99

image_path="${TOP_SRCDIR}/tests/data/inputs/formats/snake-bmp3-rgb.bmp"
reference_path="${TOP_SRCDIR}/tests/data/inputs/formats/snake-64-reference-rgb.png"
output_sixel="${ARTIFACT_LOCAL_DIR}/wic_bmp3_rgb.six"
run_img2sixel -Lwic! "${image_path}" >"${output_sixel}" || {
    fail 1 "wic bmp3 rgb conversion failed"
    exit 0
}

lsqa_err=$(
    set +xv
    run_lsqa -b "MS-SSIM:${lsqa_floor}" "${reference_path}" "${output_sixel}" 2>&1
) || lsqa_run_status=$?

test "${lsqa_run_status:-0}" -eq 0 && {
    pass 1 "wic bmp3 rgb quality meets baseline"
    exit 0
}

test "${lsqa_run_status}" -eq 5 && {
    fail 1 "${lsqa_err}"
    exit 0
}

fail 1 "wic bmp3 rgb quality regressed"

exit 0
