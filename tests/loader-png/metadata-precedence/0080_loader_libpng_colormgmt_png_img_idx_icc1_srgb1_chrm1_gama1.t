#!/bin/sh
# Policy: docs/loader/png-color-metadata.md
# A usable ICC profile wins even when contradictory lower-priority chunks exist.
# TAP test: libpng loader independent decoded color values for idx/img_idx_icc1_srgb1_chrm1_gama1.png

set -eux

test "${HAVE_LIBPNG-}" = 1 || {
    printf "1..0 # SKIP libpng support is disabled in this build\n"
    exit 0
}



echo "1..1"
set -v

input_png="${TOP_SRCDIR}/tests/data/colormgmt/input/png/idx/img_idx_icc1_srgb1_chrm1_gama1.png"
reference_image="${TOP_SRCDIR}/tests/data/colormgmt/reference/png/idx/img_idx_icc1_srgb1_chrm1_gama1.ppm"

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

echo "ok" 1 - "libpng colormgmt matches reference: idx/img_idx_icc1_srgb1_chrm1_gama1.png"
exit 0
