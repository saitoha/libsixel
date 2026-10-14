#!/bin/sh
# Verify composited color samples with explicit unmanaged sRGB input.
# Reference generation: python3 tests/data/loader/pngsuite_expected/generate_sample_references.py

set -eux

test "${HAVE_LIBPNG-}" = 1 || {
    printf '1..0 # SKIP libpng support is disabled in this build\n'
    exit 0
}
printf '1..1\n'
set -v

# Check linear alpha composition before quantization, including zero-alpha
# pixels and precedence over embedded bKGD. PPM16 rounding and float arithmetic
# fit within 0.004 byte levels. CMS-off input is explicitly interpreted as sRGB.
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/image_white_values \
    "${TOP_SRCDIR}/tests/data/loader/pngsuite_expected/bgwn6a08-srgb-white.ppm" \
    "${TOP_SRCDIR}/images/pngsuite/background/bgwn6a08.png" 'libpng:cms_engine=none!' 0.004 || {
    echo "not ok 1 - composited color samples differ from independent values"
    exit 0
}
echo "ok 1 - composited color samples match independent values"
exit 0
