#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Policy: docs/loader/builtin/tga.md
# Read packed RGB16 palette entries directly after the image ID.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "loader/0241_tga_origin_rgb16_numeric" 1>&2 || {
    echo "not ok 1 - Read packed RGB16 palette entries directly after the image ID"
    exit 0
}
echo "ok 1 - Read packed RGB16 palette entries directly after the image ID"
exit 0
