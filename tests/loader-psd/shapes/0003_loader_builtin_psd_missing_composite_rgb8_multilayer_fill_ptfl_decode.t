#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
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

input_psd="${TOP_SRCDIR}/tests/data/inputs/formats/snake16_rgb8_missing_composite_multilayer_fill_ptfl.psd"
reference_ppm="${TOP_SRCDIR}/tests/data/loader/builtin_expected/psd_snake16_multilayer_fill_ptfl_expected.ppm"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L builtin! "${input_psd}" >/dev/null || {
    echo "not ok" 1 - "builtin loader failed to decode RGB8 fill(PtFl) PSD"
    exit 0
}

# This contract concerns decoded samples, before palette quantization.
# The independent 8-bit PPM and decoded 8-bit samples can each round by
# half a level; allow one RGB byte level plus float conversion roundoff.
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/image_values \
    "${reference_ppm}" "${input_psd}" 'builtin:cms_engine=auto!' 1.01 || {
    echo "not ok 1 - decoded PSD samples differ from the independent PPM"
    exit 0
}

echo "ok" 1 - "RGB8 fill(PtFl) decode matches reference pixel values"
exit 0
