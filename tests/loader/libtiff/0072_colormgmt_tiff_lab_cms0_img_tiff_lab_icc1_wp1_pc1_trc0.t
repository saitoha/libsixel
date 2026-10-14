#!/bin/sh
# TAP test: libtiff colormgmt parity for lab/img_tiff_lab_icc1_wp1_pc1_trc0.tiff with cms=0

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
    "${TOP_SRCDIR}/tests/data/colormgmt/reference/tiff/lab/samples-unmanaged.ppm" \
    "${TOP_SRCDIR}/tests/data/colormgmt/input/tiff/lab/img_tiff_lab_icc1_wp1_pc1_trc0.tiff" 'libtiff:cms_engine=none!' 0.004 || {
    echo "not ok 1 - TIFF color metadata samples differ from independent values"
    exit 0
}
echo "ok 1 - TIFF color metadata samples match independent values"
exit 0
