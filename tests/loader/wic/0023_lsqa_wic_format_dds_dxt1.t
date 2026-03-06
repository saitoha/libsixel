#!/bin/sh
# Verify WIC DDS DXT1 decoding quality with an MS-SSIM baseline.
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

printf '1..1\n'
set -v
mkdir "${ARTIFACT_LOCAL_DIR}"

lsqa_floor=0.95

image_path="${TOP_SRCDIR}/tests/data/inputs/formats/snake-dds-dxt1.dds"
reference_path="${TOP_SRCDIR}/tests/data/inputs/formats/snake-64-reference-rgb.ppm"
output_sixel="${ARTIFACT_LOCAL_DIR}/wic_dds_dxt1.six"
run_img2sixel -Lwic! "${image_path}" >"${output_sixel}" || {
    echo "not ok" 1 - "wic dds dxt1 conversion failed"
    exit 0
}

lsqa_err=$(
    set +xv
    run_lsqa -b "MS-SSIM:${lsqa_floor}" "${reference_path}" "${output_sixel}" 2>&1
) || lsqa_run_status=$?

test "${lsqa_run_status:-0}" -eq 0 && {
    echo "ok" 1 - "wic dds dxt1 quality meets baseline"
    exit 0
}

test "${lsqa_run_status}" -eq 5 && {
    echo "not ok" 1 - "${lsqa_err}"
    exit 0
}

echo "not ok" 1 - "wic dds dxt1 quality regressed"

exit 0
