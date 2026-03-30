#!/bin/sh
# Verify RGB8 multi-layer fallback renders PtFl non-pixel fill payload.
# Reference generation commands:
#   python3 tests/data/inputs/formats/generate_psd_snake16_fixtures.py
#   magick -size 16x16 xc:'rgb(250,250,250)' -fill 'rgb(30,30,30)' \
#       -draw 'rectangle 4,0 7,3 rectangle 12,0 15,3 rectangle 0,4 3,7 rectangle 8,4 11,7 rectangle 4,8 7,11 rectangle 12,8 15,11 rectangle 0,12 3,15 rectangle 8,12 11,15' \
#       -depth 8 -define ppm:format=raw \
#       PPM:tests/data/loader/builtin_expected/psd_snake16_multilayer_fill_ptfl_expected.ppm

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

input_psd="${TOP_SRCDIR}/tests/data/inputs/formats/snake16_rgb8_missing_composite_multilayer_fill_ptfl.psd"
reference_ppm="${TOP_SRCDIR}/tests/data/loader/builtin_expected/psd_snake16_multilayer_fill_ptfl_expected.ppm"
output_sixel="${ARTIFACT_LOCAL_DIR}/psd_missing_composite_rgb8_multilayer_fill_ptfl_output.six"
lsqa_floor=${LSQA_MS_SSIM_FLOOR:-0.995}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L builtin! "${input_psd}" >"${output_sixel}" || {
    echo "not ok" 1 - "builtin loader failed to decode RGB8 fill(PtFl) PSD"
    exit 0
}

lsqa_msg=$(set +xv; ${SIXEL_RUNTIME-} "${LSQA_PATH}" -m MS-SSIM -b "MS-SSIM:${lsqa_floor}" \
    "${reference_ppm}" "${output_sixel}" 2>&1) || {
    echo "not ok" 1 - "RGB8 fill(PtFl) decode fell below MS-SSIM ${lsqa_floor}: ${lsqa_msg}"
    exit 0
}

echo "ok" 1 - "RGB8 fill(PtFl) decode keeps MS-SSIM ${lsqa_floor}"
exit 0
