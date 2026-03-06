#!/bin/sh
# Verify WIC ICO multi-size decoding quality with an MS-SSIM baseline.
# Reproduction command (ImageMagick):
#   convert tests/data/inputs/snake_64.png -define icon:auto-resize=16,32,48,64 tests/data/inputs/formats/snake-ico-multisize.ico

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
mkdir -p "${ARTIFACT_LOCAL_DIR}"

lsqa_floor=0.96

image_path="${TOP_SRCDIR}/tests/data/inputs/formats/snake-ico-multisize.ico"
reference_path="${TOP_SRCDIR}/tests/data/inputs/formats/snake-32.ppm"
output_sixel="${ARTIFACT_LOCAL_DIR}/wic_ico_multisize.six"
run_img2sixel -Lwic:ico_minsize=30! "${image_path}" >"${output_sixel}" || {
    echo "not ok" 1 - "wic ico multisize conversion failed"
    exit 0
}

lsqa_err=$(
    run_lsqa -b "MS-SSIM:${lsqa_floor}" "${reference_path}" "${output_sixel}" 2>&1
) || lsqa_run_status=$?

test "${lsqa_run_status:-0}" -eq 0 && {
    echo "ok" 1 - "wic ico multisize quality meets baseline"
    exit 0
}

test "${lsqa_run_status}" -eq 5 && {
    echo "not ok" 1 - "${lsqa_err}"
    exit 0
}

echo "not ok" 1 - "wic ico multisize quality regressed"
exit 0
