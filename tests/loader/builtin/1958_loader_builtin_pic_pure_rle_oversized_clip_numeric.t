#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Policy: docs/loader/builtin/pic.md
# Verify PIC pure-RLE oversized-run clipping with a minimal in-memory stream.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "loader/0090_loader_builtin_pic_pure_rle_oversized_clip_numeric" || {
    echo "not ok 1 - PIC pure-RLE oversized-run clipping"
    exit 0
}

echo "ok 1 - PIC pure-RLE oversized-run clipping"
exit 0
