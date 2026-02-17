#!/bin/sh
# Verify WIC PNG grayscale 16-bit decoding quality with an MS-SSIM baseline.
# Reproduction command (ImageMagick):
#   convert tests/data/inputs/snake_64.png -colorspace Gray -depth 16 tests/data/inputs/formats/snake-png-gray16.png

set -eu

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"
feature_defined_in_config "HAVE_WIC" || skip_all "wic loader is unavailable"

image_path="${TOP_SRCDIR}/tests/data/inputs/formats/snake-png-gray16.png"

set +e
probe_output=$(run_img2sixel -Lwic! "${image_path}" -o/dev/null 2>&1)
probe_status=$?
set -e

printf '%s' "${probe_output}" |
grep -q "{cacaf262-9370-4615-a13b-9f5539da4c0a} not registered" && skip_all "WIC is not available"

test "${probe_status}" -eq 0 || skip_all "wic png gray16 codec is unavailable"

lsqa_floor=${LSQA_MS_SSIM_FLOOR_WIC_PNG_GRAY16:-0.99}

printf '1..1
'
set -v

reference_path="${TOP_SRCDIR}/tests/data/inputs/formats/snake-64-reference-gray.png"
output_sixel="${ARTIFACT_LOCAL_DIR}/wic_png_gray16.six"
run_img2sixel -Lwic! "${image_path}" >"${output_sixel}" || {
    fail 1 "wic png gray16 conversion failed"
    exit 0
}

lsqa_err=$(
    set +xv
    run_lsqa -b "MS-SSIM:${lsqa_floor}" "${reference_path}" "${output_sixel}" 2>&1
) || lsqa_run_status=$?

test "${lsqa_run_status:-0}" -eq 0 && {
    pass 1 "wic png gray16 quality meets baseline"
    exit 0
}

test "${lsqa_run_status}" -eq 5 && {
    fail 1 "${lsqa_err}"
    exit 0
}

fail 1 "wic png gray16 quality regressed"

exit 0
