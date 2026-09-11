#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Policy: docs/loader/builtin/pic.md
# Verify PIC mixed-RLE repeated count 128 boundary with a minimal in-memory stream.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "loader/0092_loader_builtin_pic_mixed_rle_repeat128_numeric" || {
    echo "not ok 1 - PIC mixed-RLE repeated count 128 boundary"
    exit 0
}

echo "ok 1 - PIC mixed-RLE repeated count 128 boundary"
exit 0
