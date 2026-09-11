#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Policy: docs/loader/builtin/tga.md
# Verify TGA 8-bit grayscale palette entry with a minimal in-memory stream.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    --env "SIXEL_TEST_BUILTIN_EDGE_CASE=tga-palette8" \
    "loader/0061_loader_builtin_gif_tga_pic_edges" || {
    echo "not ok 1 - TGA 8-bit grayscale palette entry"
    exit 0
}

echo "ok 1 - TGA 8-bit grayscale palette entry"
exit 0
