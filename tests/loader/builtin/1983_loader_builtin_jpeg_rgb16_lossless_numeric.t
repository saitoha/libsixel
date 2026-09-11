#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Policy: docs/loader/builtin/jpeg.md
# Fix decoded float samples from 16-bit lossless JPEG input.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "loader/0115_loader_builtin_jpeg_rgb16_lossless_numeric" || {
    echo "not ok 1 - JPEG RGB16 lossless numeric samples"
    exit 0
}

echo "ok 1 - JPEG RGB16 lossless numeric samples"
exit 0
