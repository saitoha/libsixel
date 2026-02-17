#!/bin/sh
# Verify WIC ICO multi-size decoding quality with an MS-SSIM baseline.
# Reproduction command (ImageMagick):
#   convert tests/data/inputs/snake_64.png -define icon:auto-resize=16,32,48,64 tests/data/inputs/formats/snake-ico-multisize.ico

set -eu

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"
feature_defined_in_config "HAVE_WIC" || skip_all "wic loader is unavailable"

image_path="${TOP_SRCDIR}/tests/data/inputs/formats/snake-ico-multisize.ico"


test "${RUNTIME_ENV_IS_WINE-0}" -eq 1 && skip_all "WIC is unavailable under wine"


lsqa_floor=${LSQA_MS_SSIM_FLOOR_WIC_ICO_MULTISIZE:-0.96}

printf '1..1\n'
set -v

reference_path="${TOP_SRCDIR}/tests/data/inputs/formats/snake-32.png"
output_sixel="${ARTIFACT_LOCAL_DIR}/wic_ico_multisize.six"
run_img2sixel -Lwic:ico_minsize=40! "${image_path}" >"${output_sixel}" || {
    fail 1 "wic ico multisize conversion failed"
    exit 0
}

lsqa_err=$(
    run_lsqa -b "MS-SSIM:${lsqa_floor}" "${reference_path}" "${output_sixel}" 2>&1
) || lsqa_run_status=$?

test "${lsqa_run_status:-0}" -eq 0 && {
    pass 1 "wic ico multisize quality meets baseline"
    exit 0
}

test "${lsqa_run_status}" -eq 5 && {
    fail 1 "${lsqa_err}"
    exit 0
}

fail 1 "wic ico multisize quality regressed"
exit 0
