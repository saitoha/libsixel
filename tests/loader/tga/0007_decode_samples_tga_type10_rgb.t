#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Policy: docs/loader/builtin/tga.md
# Compare every decoded opaque TGA sample with an external decoder.
# Reference generation command (offline, ImageMagick):
#   magick tests/data/inputs/formats/snake-tga-type10-rgb.tga -alpha off -depth 8 \
#       PPM:tests/data/loader/builtin_expected/snake-tga-type10-rgb-samples.ppm
set -eux

echo "1..1"
set -v

# Both sides contain the same losslessly stored byte samples. Palette
# expansion, orientation and raw/RLE decoding therefore require exact RGB.
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/image_values \
    "${TOP_SRCDIR}/tests/data/loader/builtin_expected/snake-tga-type10-rgb-samples.ppm" \
    "${TOP_SRCDIR}/tests/data/inputs/formats/snake-tga-type10-rgb.tga" \
    'builtin:cms_engine=none!' 0 || {
    echo "not ok 1 - decoded TGA samples differ from the external reference"
    exit 0
}

echo "ok 1 - decoded TGA samples match the external reference"
exit 0
