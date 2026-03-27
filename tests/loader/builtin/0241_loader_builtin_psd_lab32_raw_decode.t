#!/bin/sh
# Verify builtin loader decodes PSD Lab 32-bit Raw with stable image quality.
# Reference generation command (coregraphics loader):
#   python3 tests/data/inputs/formats/generate_psd_snake16_fixtures.py
#   DYLD_LIBRARY_PATH=src/.libs converters/.libs/img2sixel \
#       -L coregraphics! \
#       tests/data/inputs/formats/snake16_lab32_raw.psd \
#       > tests/data/loader/builtin_expected/psd_snake16_lab32_coregraphics_expected.six

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

echo "1..1"
set -v
mkdir -p "${ARTIFACT_LOCAL_DIR}"

input_psd="${TOP_SRCDIR}/tests/data/inputs/formats/snake16_lab32_raw.psd"
reference_six="${TOP_SRCDIR}/tests/data/loader/builtin_expected/psd_snake16_lab32_coregraphics_expected.six"
output_sixel="${ARTIFACT_LOCAL_DIR}/psd_lab32_raw_output.six"
lsqa_floor=${LSQA_MS_SSIM_FLOOR:-0.99}

run_img2sixel -L builtin! "${input_psd}" >"${output_sixel}" || {
    echo "not ok" 1 - "PSD Lab 32-bit Raw decode failed"
    exit 0
}

lsqa_msg=$(set +xv; run_lsqa -m MS-SSIM -b "MS-SSIM:${lsqa_floor}" \
    "${reference_six}" "${output_sixel}" 2>&1) || {
    echo "not ok" 1 - "PSD Lab 32-bit Raw fell below MS-SSIM ${lsqa_floor}: ${lsqa_msg}"
    exit 0
}

echo "ok" 1 - "PSD Lab 32-bit Raw keeps MS-SSIM ${lsqa_floor} against expected sixel"
exit 0
