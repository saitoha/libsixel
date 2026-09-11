#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Policy: docs/loader/builtin/tga.md
# Verify TGA bottom-origin row order with a minimal in-memory stream.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    --env "SIXEL_TEST_BUILTIN_EDGE_CASE=tga-bottom-origin" \
    "loader/0061_loader_builtin_gif_tga_pic_edges" || {
    echo "not ok 1 - TGA bottom-origin row order"
    exit 0
}

echo "ok 1 - TGA bottom-origin row order"
exit 0
