#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Policy: docs/loader/builtin/gif.md
# Verify GIF image rectangle offset composition with a minimal in-memory stream.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "loader/0073_loader_builtin_gif_rectangle_offset_numeric" || {
    echo "not ok 1 - GIF image rectangle offset composition"
    exit 0
}

echo "ok 1 - GIF image rectangle offset composition"
exit 0
