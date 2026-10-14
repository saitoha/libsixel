#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Verify decoded palette PNG RGB values before encoder quantization.
set -eux

echo "1..1"
set -v

# The fixed PPM contains the independent original RGB byte samples.
# This asserts color placement; alpha semantics have separate coverage.
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/image_values \
    "${TOP_SRCDIR}/tests/data/loader/builtin_expected/0004_palette_png_default_black_composite.ppm" \
    "${TOP_SRCDIR}/tests/data/inputs/formats/palette.png" \
    'builtin:cms_engine=none!' 0 || {
    echo "not ok 1 - decoded palette PNG RGB values differ from the reference"
    exit 0
}
echo "ok 1 - decoded palette PNG RGB values match the reference"
exit 0
