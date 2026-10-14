#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Verify decoded Softimage PIC RGB values before encoder quantization.
set -eux

echo "1..1"
set -v

# The fixed PPM contains the independent original RGB byte samples.
# This asserts color placement; alpha semantics have separate coverage.
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/image_values \
    "${TOP_SRCDIR}/tests/data/loader/builtin_expected/pic_snake16_reference.ppm" \
    "${TOP_SRCDIR}/tests/data/inputs/formats/pic_valid_snake16_raw_rgb.pic" \
    'builtin:cms_engine=none!' 0 || {
    echo "not ok 1 - decoded Softimage PIC RGB values differ from the reference"
    exit 0
}
echo "ok 1 - decoded Softimage PIC RGB values match the reference"
exit 0
