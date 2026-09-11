#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Policy: docs/loader/builtin/psd.md
# Fix decoded float samples from PSD 32-bit PackBits RLE.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "loader/0117_loader_builtin_psd_rgb32_rle_numeric" || {
    echo "not ok 1 - PSD RGB32 RLE numeric samples"
    exit 0
}

echo "ok 1 - PSD RGB32 RLE numeric samples"
exit 0
