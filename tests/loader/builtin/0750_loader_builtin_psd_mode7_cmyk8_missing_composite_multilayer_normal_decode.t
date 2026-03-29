#!/bin/sh
# Verify builtin loader reconstructs mode7(4ch->CMYK8) multi-layer PSD
# (missing composite) with normal blend and layer offsets.
# Reference generation commands:
#   python3 tests/data/inputs/formats/generate_psd_snake16_fixtures.py
#   magick tests/data/loader/builtin_expected/psd_snake16_cmyk8_expected.ppm \
#       \( -size 16x16 xc:none -fill black -draw 'rectangle 4,4 11,11' \) \
#       -compose over -composite -depth 8 -define ppm:format=raw \
#       PPM:tests/data/loader/builtin_expected/psd_snake16_multilayer_cmyk8_normal_expected.ppm

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

input_psd="${TOP_SRCDIR}/tests/data/inputs/formats/snake16_mode7_cmyk8_missing_composite_multilayer_normal.psd"
reference_ppm="${TOP_SRCDIR}/tests/data/loader/builtin_expected/psd_snake16_multilayer_cmyk8_normal_expected.ppm"
output_sixel="${ARTIFACT_LOCAL_DIR}/psd_mode7_missing_composite_multilayer_cmyk8_normal_output.six"
lsqa_floor=${LSQA_MS_SSIM_FLOOR:-0.995}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L builtin! "${input_psd}" >"${output_sixel}" || {
    echo "not ok" 1 - "builtin loader failed to decode mode7 multi-layer PSD CMYK8(normal)"
    exit 0
}

lsqa_msg=$(set +xv; ${SIXEL_RUNTIME-} "${LSQA_PATH}" -m MS-SSIM -W linear -b "MS-SSIM:${lsqa_floor}" \
    "${reference_ppm}" "${output_sixel}" 2>&1) || {
    echo "not ok" 1 - "mode7 multi-layer PSD CMYK8(normal) fell below MS-SSIM ${lsqa_floor}: ${lsqa_msg}"
    exit 0
}

echo "ok" 1 - "mode7 multi-layer PSD CMYK8(normal) keeps MS-SSIM ${lsqa_floor} against expected PPM"
exit 0
