#!/bin/sh
# Policy: docs/loader/png-color-metadata.md
# Test-plan: docs/testing/builtin-loader-coverage.md
# TAP test: builtin loader independent decoded color values for rgb/img_rgb_icc0_srgb0_chrm0_gama0.png

set -eux



echo "1..1"
set -v

input_png="${TOP_SRCDIR}/tests/data/colormgmt/input/png/rgb/img_rgb_icc0_srgb0_chrm0_gama0.png"
reference_image="${TOP_SRCDIR}/tests/data/colormgmt/reference/png/rgb/img_rgb_icc0_srgb0_chrm0_gama0.ppm"

# Check every displayed sRGB component against source equations before
# palette quantization. Half an 8-bit level plus reference/float roundoff
# permits either byte or float32 decoder storage without a spatial metric.
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/image_display_values \
    "${reference_image}" "${input_png}" 'builtin:cms_engine=auto!' 0.51 || {
    echo "not ok 1 - decoded PNG color samples differ from the independent oracle"
    exit 0
}

echo "ok" 1 - "builtin colormgmt matches reference: rgb/img_rgb_icc0_srgb0_chrm0_gama0.png"
exit 0
