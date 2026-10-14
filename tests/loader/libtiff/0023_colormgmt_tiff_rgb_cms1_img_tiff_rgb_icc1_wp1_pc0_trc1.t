#!/bin/sh
# TAP test: libtiff colormgmt parity for rgb/img_tiff_rgb_icc1_wp1_pc0_trc1.tiff

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
# Regenerate: python3 tools/generate_cms_quality_references.py
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/image_display_values \
    "${TOP_SRCDIR}/tests/data/colormgmt/reference/tiff/rgb/samples-managed.ppm" \
    "${TOP_SRCDIR}/tests/data/colormgmt/input/tiff/rgb/img_tiff_rgb_icc1_wp1_pc0_trc1.tiff" 'libtiff:cms_engine=auto!' 0.004 || {
    echo "not ok 1 - TIFF color metadata samples differ from independent values"
    exit 0
}
echo "ok 1 - TIFF color metadata samples match independent values"
exit 0
