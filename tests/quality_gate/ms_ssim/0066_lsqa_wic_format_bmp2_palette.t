#!/bin/sh
# Verify WIC BMP2 palette decoding quality with an MS-SSIM baseline.
# Reproduction command (ImageMagick):
#   convert tests/data/inputs/snake_64.png -colors 256 BMP2:tests/data/inputs/formats/snake-bmp2-pal8.bmp
#   convert tests/data/inputs/snake_64.png BMP3:tests/data/inputs/formats/snake-bmp3-rgb.bmp
#   convert tests/data/inputs/snake_64.png -interlace Plane tests/data/inputs/formats/snake-gif-interlaced.gif
#   convert tests/data/inputs/snake_64.png -colors 256 tests/data/inputs/formats/snake-ico-pal8.ico
#   convert tests/data/inputs/formats/rgba.png tests/data/inputs/formats/snake-ico-rgba.ico
#   convert tests/data/inputs/snake_64.png DXT1:tests/data/inputs/formats/snake-dds-dxt1.dds
#   convert tests/data/inputs/formats/rgba.png DXT5:tests/data/inputs/formats/snake-dds-dxt5.dds

set -eu

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"
feature_defined_in_config "HAVE_WIC" || skip_all "wic loader is unavailable"

image_path="${TOP_SRCDIR}/tests/data/inputs/formats/snake-bmp2-pal8.bmp"

set +e
probe_output=$(run_img2sixel -Lwic! "${image_path}" -o/dev/null 2>&1)
probe_status=$?
set -e

printf '%s' "${probe_output}" |
grep -q "{cacaf262-9370-4615-a13b-9f5539da4c0a} not registered" && skip_all "WIC is not available"

test "${probe_status}" -eq 0 || skip_all "wic bmp2 palette codec is unavailable"

lsqa_floor=${LSQA_MS_SSIM_FLOOR_WIC_BMP2_PALETTE:-0.99}

printf '1..1\n'

set -v

reference_path="${TOP_SRCDIR}/tests/data/inputs/formats/snake-64-reference-rgb.png"
output_sixel="${ARTIFACT_LOCAL_DIR}/wic_bmp2_palette.six"
run_img2sixel -Lwic! "${image_path}" >"${output_sixel}" || {
    fail 1 "wic bmp2 palette conversion failed"
    exit 0
}

lsqa_err=$(
    set +xv
    run_lsqa -b "MS-SSIM:${lsqa_floor}" "${reference_path}" "${output_sixel}" 2>&1
) || lsqa_run_status=$?

test "${lsqa_run_status:-0}" -eq 0 && {
    pass 1 "wic bmp2 palette quality meets baseline"
    exit 0
}

test "${lsqa_run_status}" -eq 5 && {
    fail 1 "${lsqa_err}"
    exit 0
}

fail 1 "wic bmp2 palette quality regressed"

exit 0
