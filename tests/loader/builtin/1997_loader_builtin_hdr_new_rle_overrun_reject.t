#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Policy: docs/loader/builtin/hdr.md
# Verify a component-RLE run cannot exceed the scanline.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "loader/0131_loader_builtin_hdr_new_rle_overrun_reject" || {
    echo "not ok 1 - HDR component RLE rejects a scanline overrun"
    exit 0
}

echo "ok 1 - HDR component RLE rejects a scanline overrun"
exit 0
