#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Policy: docs/loader/builtin/tga.md
# Use the full sixteen-bit colormap origin in RLE packets.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "loader/0233_tga_origin_index16_rle_numeric" 1>&2 || {
    echo "not ok 1 - Use the full sixteen-bit colormap origin in RLE packets"
    exit 0
}
echo "ok 1 - Use the full sixteen-bit colormap origin in RLE packets"
exit 0
