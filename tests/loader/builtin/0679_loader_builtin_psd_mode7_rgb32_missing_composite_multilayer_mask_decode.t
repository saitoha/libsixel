#!/bin/sh
# Verify builtin loader applies raster user-mask channel (-2) in a layer-only
# mode7(3ch->RGB32) multi-layer PSD.
# Reference generation commands:
#   python3 tests/data/inputs/formats/generate_psd_snake16_fixtures.py
#   # tests/data/loader/builtin_expected/psd_snake16_alpha8_fade_mask.pgm is a
#   # static radial alpha mask generated offline from the fixture formula.
#   magick -size 16x16 xc:'rgb(0,255,0)' \
#       tests/data/loader/builtin_expected/psd_snake16_alpha8_fade_mask.pgm \
#       -alpha off -compose copy_opacity -composite /tmp/psd_multilayer_mask_overlay_rgb32.png
#   magick tests/data/loader/builtin_expected/psd_snake16_rgb32_expected.ppm \
#       -colorspace RGB /tmp/psd_multilayer_mask_overlay_rgb32.png -colorspace RGB \
#       -compose over -composite -colorspace sRGB \
#       -depth 8 -define ppm:format=raw \
#       PPM:tests/data/loader/builtin_expected/psd_snake16_multilayer_rgb32_mask_expected.ppm

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v
mkdir -p "${ARTIFACT_LOCAL_DIR}"

input_psd="${TOP_SRCDIR}/tests/data/inputs/formats/snake16_mode7_rgb32_missing_composite_multilayer_mask.psd"
reference_ppm="${TOP_SRCDIR}/tests/data/loader/builtin_expected/psd_snake16_multilayer_rgb32_mask_expected.ppm"
output_sixel="${ARTIFACT_LOCAL_DIR}/psd_mode7_missing_composite_multilayer_rgb32_mask_output.six"
lsqa_floor=${LSQA_MS_SSIM_FLOOR:-0.995}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L builtin! "${input_psd}" >"${output_sixel}" || {
    echo "not ok" 1 - "builtin loader failed to decode mode7 multi-layer PSD RGB32(mask)"
    exit 0
}

lsqa_msg=$(set +xv; ${SIXEL_RUNTIME-} "${LSQA_PATH}" -m MS-SSIM -b "MS-SSIM:${lsqa_floor}" \
    "${reference_ppm}" "${output_sixel}" 2>&1) || {
    echo "not ok" 1 - "mode7 multi-layer PSD RGB32(mask) fell below MS-SSIM ${lsqa_floor}: ${lsqa_msg}"
    exit 0
}

echo "ok" 1 - "mode7 multi-layer PSD RGB32(mask) keeps MS-SSIM ${lsqa_floor} against expected PPM"
exit 0
