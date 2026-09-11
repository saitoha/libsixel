#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Policy: docs/loader/builtin/tga.md
# Verify TGA 16-bit palette entry expansion with a minimal in-memory stream.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    --env "SIXEL_TEST_BUILTIN_EDGE_CASE=tga-palette16" \
    "loader/0061_loader_builtin_gif_tga_pic_edges" || {
    echo "not ok 1 - TGA 16-bit palette entry expansion"
    exit 0
}

echo "ok 1 - TGA 16-bit palette entry expansion"
exit 0
