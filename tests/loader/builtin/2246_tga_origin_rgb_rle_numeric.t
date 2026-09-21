#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Policy: docs/loader/builtin/tga.md
# Expand raw and run RLE packets relative to the colormap origin.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "loader/0230_tga_origin_rgb_rle_numeric" 1>&2 || {
    echo "not ok 1 - Expand raw and run RLE packets relative to the colormap origin"
    exit 0
}
echo "ok 1 - Expand raw and run RLE packets relative to the colormap origin"
exit 0
