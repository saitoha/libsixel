#!/bin/sh
# Verify WIC GIF palette decoding quality with an MS-SSIM baseline.

set -eu

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"
feature_defined_in_config "HAVE_WIC" || skip_all "wic loader is unavailable"

set +e
probe_output=$(run_img2sixel -Lwic! "${TOP_SRCDIR}/tests/data/inputs/snake_64.png" >/dev/null 2>&1)
probe_status=$?
set -e

printf '%s' "${probe_output}" \
    | grep "{cacaf262-9370-4615-a13b-9f5539da4c0a} not registered" \
    >/dev/null && skip_all "WIC is not available"

test "${probe_status}" -eq 0 || skip_all "WIC probe failed"

set +e
snake_gif_probe_output=$(run_img2sixel -Lwic! "${TOP_SRCDIR}/tests/data/inputs/snake_64.gif" >/dev/null 2>&1)
snake_gif_probe_status=$?
run_img2sixel -Lwic! "${TOP_SRCDIR}/tests/data/inputs/formats/snake-gif-interlaced.gif" >/dev/null 2>&1
interlaced_gif_probe_status=$?
set -e

printf '%s' "${snake_gif_probe_output}" \
    | grep "type      : unknown content" \
    >/dev/null \
    && test "${snake_gif_probe_status}" -ne 0 \
    && test "${interlaced_gif_probe_status}" -eq 0 \
    && skip_all "WIC rejected snake_64.gif as unknown content"

lsqa_floor=${LSQA_MS_SSIM_FLOOR_WIC_GIF_PALETTE:-0.98}

printf '1..1\n'
set -v

image_path="${TOP_SRCDIR}/tests/data/inputs/snake_64.gif"
reference_path="${TOP_SRCDIR}/tests/data/inputs/formats/snake-64-reference-rgb.png"
output_sixel="${ARTIFACT_LOCAL_DIR}/wic_gif_palette.six"
run_img2sixel -Lwic! "${image_path}" >"${output_sixel}" || {
    fail 1 "wic gif palette conversion failed"
    exit 0
}

lsqa_err=$(
    set +xv
    run_lsqa -b "MS-SSIM:${lsqa_floor}" "${reference_path}" "${output_sixel}" 2>&1
) || lsqa_run_status=$?

test "${lsqa_run_status:-0}" -eq 0 && {
    pass 1 "wic gif palette quality meets baseline"
    exit 0
}

test "${lsqa_run_status}" -eq 5 && {
    fail 1 "${lsqa_err}"
    exit 0
}

fail 1 "wic gif palette quality regressed"

exit 0
