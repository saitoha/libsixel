#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Verify CMYK8 multi-layer fallback renders PtFl descriptor Grsc payload.
# Reference generation commands:
#   python3 tests/data/inputs/formats/generate_psd_snake16_fixtures.py
#   (expected PPM is shared with 0773 checker-style PtFl output)

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

input_psd="${TOP_SRCDIR}/tests/data/inputs/formats/snake16_cmyk8_missing_composite_multilayer_fill_ptfl_descriptor_gray.psd"
reference_ppm="${TOP_SRCDIR}/tests/data/loader/builtin_expected/psd_snake16_multilayer_fill_ptfl_expected.ppm"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L builtin! "${input_psd}" >/dev/null || {
    echo "not ok" 1 - "builtin loader failed to decode CMYK8 fill(PtFl descriptor Grsc) PSD"
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

echo "ok" 1 - "CMYK8 fill(PtFl descriptor Grsc) decode matches reference pixel values"
exit 0
