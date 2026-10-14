#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Verify decoded samples against the independent reference.
# and keeps stable quality against a fixed expected PNM reference.
# Reference generation command (ImageMagick):
#   magick tests/data/inputs/formats/snake-jpeg-16bit-lossless-gray-restart.jpg \
#       -depth 8 -define ppm:format=raw \
#       PPM:tests/data/loader/builtin_expected/0008_jpeg_gray_16bit_lossless_r64_reference.ppm

set -eux

printf '1..1\n'
set -v

# Compare decoded values before palette quantization. The budget covers
# reference rounding and decoder storage rounding, not spatial pooling.
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/image_display_values \
    "${TOP_SRCDIR}/tests/data/loader/builtin_expected/0008_jpeg_gray_16bit_lossless_r64_reference.ppm" \
    "${TOP_SRCDIR}/tests/data/inputs/formats/snake-jpeg-16bit-lossless-gray-restart.jpg" 'builtin:Eauto!' 0.51 || {
    echo "not ok 1 - decoded samples differ from the reference"
    exit 0
}

echo "ok 1 - decoded samples match the independent reference"
exit 0
