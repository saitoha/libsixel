#!/bin/sh
# Verify libtiff builtin CMS output for RGB parametric ICC fixture (types 0/1/2).

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

# Both build systems publish this loader capability as HAVE_LIBTIFF.
test "${HAVE_LIBTIFF-}" = 1 || {
    printf "1..0 # SKIP libtiff loader is unavailable\n"
    exit 0
}

echo "1..1"
set -v

# Independent parametric ICC equations; compare before palette quantization.
# Regenerate: python3 tools/generate_cms_quality_references.py
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/image_display_values \
    "${TOP_SRCDIR}/tests/data/colormgmt/reference/custom/rgb_parametric_012_tiff_analytic.ppm" \
    "${TOP_SRCDIR}/tests/data/colormgmt/input/custom/rgb_parametric_012.tiff" \
    'libtiff:cms_engine=builtin!' 0.004 || {
    echo "not ok 1 - TIFF parametric ICC samples differ from independent values"
    exit 0
}
echo "ok 1 - TIFF parametric ICC samples match independent values"
exit 0
