#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Policy: docs/loader/builtin/psd.md
# Verify builtin PSD ICC conversion matches a fixed no-ICC PNM reference.

set -eux

test "${HAVE_LCMS2-}" = 1 || {
    printf "1..0 # SKIP lcms2 support is disabled in this build\n"
    exit 0
}


echo "1..1"
set -v

input_psd="${TOP_SRCDIR}/tests/data/inputs/formats/snake-64-embedded-esrgb.psd"
reference_ppm="${TOP_SRCDIR}/tests/data/loader/builtin_expected/0002_snake_64_embedded_esrgb_converted_srgb_noicc.ppm"

# The external 8-bit reference and the loader byte path can each round
# once; one RGB level plus float roundoff bounds this sample contract.
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/image_values \
    "${reference_ppm}" "${input_psd}" 'builtin:cms_engine=auto!' 1.01 || {
    echo "not ok 1 - decoded PSD ICC samples differ from the external reference"
    exit 0
}
echo "ok 1 - decoded PSD ICC samples match the external reference"
exit 0
