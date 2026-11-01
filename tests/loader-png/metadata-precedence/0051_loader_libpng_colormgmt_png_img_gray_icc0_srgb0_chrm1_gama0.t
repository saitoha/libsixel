#!/bin/sh
# Policy: docs/loader/png-color-metadata.md
# TAP test: libpng loader independent decoded color values for gray/img_gray_icc0_srgb0_chrm1_gama0.png

set -eux

test "${HAVE_LIBPNG-}" = 1 || {
    printf "1..0 # SKIP libpng support is disabled in this build\n"
    exit 0
}



echo "1..1"
set -v

input_png="${TOP_SRCDIR}/tests/data/colormgmt/input/png/gray/img_gray_icc0_srgb0_chrm1_gama0.png"
reference_image="${TOP_SRCDIR}/tests/data/colormgmt/reference/png/gray/img_gray_icc0_srgb0_chrm1_gama0.ppm"

test -f "${input_png}" || {
    echo "not ok" 1 - "missing test fixture"
    exit 0
}

test -f "${reference_image}" || {
    echo "not ok" 1 - "missing test fixture"
    exit 0
}

# Check every displayed sRGB component against source equations before
# palette quantization. Half an 8-bit level plus reference/float roundoff
# permits either byte or float32 decoder storage without a spatial metric.
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/image_display_values \
    "${reference_image}" "${input_png}" 'libpng:cms_engine=auto!' 0.51 || {
    echo "not ok 1 - decoded PNG color samples differ from the independent oracle"
    exit 0
}

echo "ok" 1 - "libpng colormgmt matches reference: gray/img_gray_icc0_srgb0_chrm1_gama0.png"
exit 0
