#!/bin/sh
# Verify exact crop coordinates and decoded color samples.
# Reference generation: python3 tests/data/loader/pngsuite_expected/generate_sample_references.py

set -eux

test "${HAVE_LIBPNG-}" = 1 || {
    printf "1..0 # SKIP libpng support is disabled in this build\n"
    exit 0
}

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

echo "1..1"
set -v

input_png="${TOP_SRCDIR}/images/pngsuite/basic/basn4a08.png"
expected_ppm="${TOP_SRCDIR}/tests/data/loader/pngsuite_expected/basn4a08-srgb-black-crop.ppm"
output_sixel="${TMPDIR:-/tmp}/libsixel-${0##*/}-$$-basn4a08.sixel"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --binning-policy=exact --sampling-policy=full-frame --lookup-policy=none -dnone -Acomposite -B#000 -Nexplicit_first --env SIXEL_LOADER_LIBPNG_USE_TRNS_KEYCOLOR=0 -c16x16+8+8 -Llibpng:cms_engine=none! "${input_png}" >"${output_sixel}" || {
    echo "not ok" 1 - "img2sixel failed"
    exit 0
}

# At most 256 cropped samples fit the exact palette. Allow half an 8-bit
# palette level plus half a SIXEL percentage step (1.275 byte levels), with
# PPM16 and float roundoff. Every sample and the 16x16 size must agree.
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/image_values \
    "${expected_ppm}" "${output_sixel}" 'builtin:Enone!' 1.8 || {
    echo "not ok 1 - cropped samples differ from the independent reference"
    exit 0
}
echo "ok 1 - crop coordinates and color samples match"
exit 0
