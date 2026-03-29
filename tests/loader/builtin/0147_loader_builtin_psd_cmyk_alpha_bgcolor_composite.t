#!/bin/sh
# Verify builtin PSD CMYK alpha compositing responds to --bgcolor.
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


echo "1..1"
set -v
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

input_psd="${TOP_SRCDIR}/tests/data/inputs/formats/snake16_cmyk8_alpha.psd"
output_black_six="${ARTIFACT_LOCAL_DIR}/psd_cmyk_alpha_bg_black.six"
output_white_six="${ARTIFACT_LOCAL_DIR}/psd_cmyk_alpha_bg_white.six"
output_black_png="${ARTIFACT_LOCAL_DIR}/psd_cmyk_alpha_bg_black.png"
output_white_png="${ARTIFACT_LOCAL_DIR}/psd_cmyk_alpha_bg_white.png"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -Lbuiltin! -B "#000000" "${input_psd}" >"${output_black_six}" || {
    echo "not ok" 1 - "builtin PSD decode failed with black bgcolor"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -Lbuiltin! -B "#ffffff" "${input_psd}" >"${output_white_six}" || {
    echo "not ok" 1 - "builtin PSD decode failed with white bgcolor"
    exit 0
}

${SIXEL_RUNTIME-} "${SIXEL2PNG_PATH}" -i "${output_black_six}" -o "${output_black_png}" || {
    echo "not ok" 1 - "black bgcolor sixel2png decode failed"
    exit 0
}

${SIXEL_RUNTIME-} "${SIXEL2PNG_PATH}" -i "${output_white_six}" -o "${output_white_png}" || {
    echo "not ok" 1 - "white bgcolor sixel2png decode failed"
    exit 0
}

lsqa_status=0
lsqa_msg=$(set +xv; ${SIXEL_RUNTIME-} "${LSQA_PATH}" -m MS-SSIM -b "MS-SSIM:0.99" \
    "${output_black_png}" "${output_white_png}" 2>&1) || lsqa_status=$?

test "${lsqa_status}" -eq 5 || {
    echo "not ok" 1 - "PSD bgcolor did not affect CMYK alpha output: ${lsqa_msg}"
    exit 0
}

echo "ok" 1 - "builtin PSD CMYK alpha compositing changes with bgcolor"
exit 0
