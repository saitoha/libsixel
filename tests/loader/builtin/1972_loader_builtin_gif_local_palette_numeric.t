#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Policy: docs/loader/builtin/gif.md
# Verify a GIF local table replaces global colors with exact pixels.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "loader/0104_loader_builtin_gif_local_palette_numeric" || {
    echo "not ok 1 - GIF local palette exact frame colors"
    exit 0
}

echo "ok 1 - GIF local palette exact frame colors"
exit 0
