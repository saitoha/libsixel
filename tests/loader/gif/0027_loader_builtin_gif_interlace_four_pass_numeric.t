#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Policy: docs/loader/builtin/gif.md
# Verify GIF four-pass interlace row order with a minimal in-memory stream.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "loader/0072_loader_builtin_gif_interlace_four_pass_numeric" || {
    echo "not ok 1 - GIF four-pass interlace row order"
    exit 0
}

echo "ok 1 - GIF four-pass interlace row order"
exit 0
