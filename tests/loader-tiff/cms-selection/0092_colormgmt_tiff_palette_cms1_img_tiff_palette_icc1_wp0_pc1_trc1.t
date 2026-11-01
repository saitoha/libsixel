#!/bin/sh
# Reference: original UINT16 ColorMap, plus stored ICC equations for cms1.
# No encoder or CMM is used to generate the unquantized reference.
# TAP test: libtiff colormgmt parity for palette/img_tiff_palette_icc1_wp0_pc1_trc1.tiff

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

test "${HAVE_LIBTIFF-}" = 1 || {
    printf "1..0 # SKIP libtiff loader is unavailable\n"
    exit 0
}


echo "1..1"
set -v

# Compare original decoded values before palette construction. Independent
# source/ICC equations use PPM16; half a reference level plus float rounding
# is below 0.004 in byte units. No dithered SIXEL golden supplies these values.
# This analytic sample contract requires builtin CMS. Auto selects a native
# CMM on some builds, whose approximation is covered by the engine-specific
# numeric tests with their own profiles and budgets, not this 0.004-byte gate.
# Regenerate: python3 tools/generate_cms_quality_references.py
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/image_display_values \
    "${TOP_SRCDIR}/tests/data/colormgmt/reference/tiff/palette/colormap-icc-srgb.ppm" \
    "${TOP_SRCDIR}/tests/data/colormgmt/input/tiff/palette/img_tiff_palette_icc1_wp0_pc1_trc1.tiff" 'libtiff:cms_engine=builtin!' 0.004 || {
    echo "not ok 1 - TIFF color metadata samples differ from independent values"
    exit 0
}
echo "ok 1 - TIFF color metadata samples match independent values"
exit 0
