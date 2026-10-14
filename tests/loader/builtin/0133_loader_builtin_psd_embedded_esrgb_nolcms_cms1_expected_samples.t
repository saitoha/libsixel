#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Verify builtin loader applies embedded PSD ICC when lcms2 is unavailable.

set -eux

test "${HAVE_LCMS2-}" != 1 || {
    printf "1..0 # SKIP lcms2 support is enabled in this build\n"
    exit 0
}



printf '1..1\n'
set -v

input_psd="${TOP_SRCDIR}/tests/data/inputs/formats/snake-64-embedded-esrgb.psd"
reference_path="${TOP_SRCDIR}/tests/data/loader/builtin_expected/0002_snake_64_embedded_esrgb_converted_srgb_noicc.ppm"

# The external 8-bit reference and the loader byte path can each round
# once; one RGB level plus float roundoff bounds this sample contract.
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/image_values \
    "${reference_path}" "${input_psd}" 'builtin:cms_engine=auto!' 1.01 || {
    echo "not ok 1 - decoded PSD ICC samples differ from the external reference"
    exit 0
}
echo "ok 1 - decoded PSD ICC samples match the external reference"
exit 0
