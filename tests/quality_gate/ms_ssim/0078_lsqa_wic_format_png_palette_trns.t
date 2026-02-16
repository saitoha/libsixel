#!/bin/sh
# Verify WIC PNG palette with transparency decoding quality with an MS-SSIM baseline.
# Reproduction command (ImageMagick):
#   convert tests/data/inputs/snake_64.png -colors 256 -transparent '#000000' PNG8:tests/data/inputs/formats/snake-png-pal8-trns.png

set -eu

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"
feature_defined_in_config "HAVE_WIC" || skip_all "wic loader is unavailable"

image_path="${TOP_SRCDIR}/tests/data/inputs/formats/snake-png-pal8-trns.png"

set +e
probe_output=$(run_img2sixel -Lwic! "${image_path}" >/dev/null 2>&1)
probe_status=$?
set -e

printf '%s' "${probe_output}"     | grep "{cacaf262-9370-4615-a13b-9f5539da4c0a} not registered"     >/dev/null && skip_all "WIC is not available"

test "${probe_status}" -eq 0 || skip_all "wic png palette trns codec is unavailable"

lsqa_floor=${LSQA_MS_SSIM_FLOOR_WIC_PNG_PALETTE_TRNS:-0.98}

printf '1..1
'
set -v

reference_path="${TOP_SRCDIR}/tests/data/inputs/formats/snake-64-reference-rgba.png"
output_sixel="${ARTIFACT_LOCAL_DIR}/wic_png_palette_trns.six"
run_img2sixel -Lwic! "${image_path}" >"${output_sixel}" || {
    fail 1 "wic png palette trns conversion failed"
    exit 0
}

lsqa_err=$(
    set +xv
    run_lsqa -b "MS-SSIM:${lsqa_floor}" "${reference_path}" "${output_sixel}" 2>&1
) || lsqa_run_status=$?

test "${lsqa_run_status:-0}" -eq 0 && {
    pass 1 "wic png palette trns quality meets baseline"
    exit 0
}

test "${lsqa_run_status}" -eq 5 && {
    fail 1 "${lsqa_err}"
    exit 0
}

fail 1 "wic png palette trns quality regressed"

exit 0
