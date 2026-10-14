#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Verify decoded samples against the independent reference.
# against a fixed expected PNM reference.
# Reference generation command (ImageMagick):
#   magick tests/data/inputs/formats/snake-jpeg-8bit-lossless.jpg \
#       -depth 8 -define ppm:format=raw \
#       PPM:tests/data/loader/builtin_expected/0007_jpeg_rgb_8bit_lossless_r0_reference.ppm

set -eux

printf '1..1\n'
set -v

# Compare decoded values before palette quantization. The budget covers
# reference rounding and decoder storage rounding, not spatial pooling.
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/image_display_values \
    "${TOP_SRCDIR}/tests/data/loader/builtin_expected/0007_jpeg_rgb_8bit_lossless_r0_reference.ppm" \
    "${TOP_SRCDIR}/tests/data/inputs/formats/snake-jpeg-8bit-lossless.jpg" 'builtin:Eauto!' 0.0001 || {
    echo "not ok 1 - decoded samples differ from the reference"
    exit 0
}

echo "ok 1 - decoded samples match the independent reference"
exit 0
