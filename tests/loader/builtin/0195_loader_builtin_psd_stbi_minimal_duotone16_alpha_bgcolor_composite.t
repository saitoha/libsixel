#!/bin/sh
# Verify builtin PSD duotone 16-bit+alpha compositing responds to --bgcolor.
# Fixture generation command:
#   python3 tests/data/inputs/formats/generate_psd_snake16_fixtures.py

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

test "${HAVE_SIXEL2PNG-}" = 1 || {
    printf "1..0 # SKIP sixel2png is disabled in this build\n"
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

echo "1..1"
set -v
mkdir -p "${ARTIFACT_LOCAL_DIR}"

input_psd="${TOP_SRCDIR}/tests/data/inputs/formats/snake16_duotone16_alpha.psd"
output_black_six="${ARTIFACT_LOCAL_DIR}/snake16_duotone16_alpha_bg_black.six"
output_white_six="${ARTIFACT_LOCAL_DIR}/snake16_duotone16_alpha_bg_white.six"
output_black_png="${ARTIFACT_LOCAL_DIR}/snake16_duotone16_alpha_bg_black.png"
output_white_png="${ARTIFACT_LOCAL_DIR}/snake16_duotone16_alpha_bg_white.png"

run_img2sixel -Lbuiltin! -B "#000000" "${input_psd}" >"${output_black_six}" || {
    echo "not ok" 1 - "builtin PSD decode failed with black bgcolor (duotone 16-bit+alpha)"
    exit 0
}

run_img2sixel -Lbuiltin! -B "#ffffff" "${input_psd}" >"${output_white_six}" || {
    echo "not ok" 1 - "builtin PSD decode failed with white bgcolor (duotone 16-bit+alpha)"
    exit 0
}

run_sixel2png -i "${output_black_six}" -o "${output_black_png}" || {
    echo "not ok" 1 - "black bgcolor sixel2png decode failed (duotone 16-bit+alpha)"
    exit 0
}

run_sixel2png -i "${output_white_six}" -o "${output_white_png}" || {
    echo "not ok" 1 - "white bgcolor sixel2png decode failed (duotone 16-bit+alpha)"
    exit 0
}

lsqa_status=0
lsqa_msg=$(set +xv; run_lsqa -m MS-SSIM -b "MS-SSIM:0.99"     "${output_black_png}" "${output_white_png}" 2>&1) || lsqa_status=$?

test "${lsqa_status}" -eq 5 || {
    echo "not ok" 1 - "PSD bgcolor did not affect output (duotone 16-bit+alpha): ${lsqa_msg}"
    exit 0
}

echo "ok" 1 - "builtin PSD duotone 16-bit+alpha compositing changes with bgcolor"
exit 0
