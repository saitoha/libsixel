#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Policy: docs/loader/builtin/gif.md
# Verify GIF LZW code-width growth through 12 bits with a minimal in-memory stream.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "loader/0079_loader_builtin_gif_lzw_12bit_width_numeric" || {
    echo "not ok 1 - GIF LZW code-width growth through 12 bits"
    exit 0
}

echo "ok 1 - GIF LZW code-width growth through 12 bits"
exit 0
