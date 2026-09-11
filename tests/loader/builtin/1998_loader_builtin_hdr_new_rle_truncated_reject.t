#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Policy: docs/loader/builtin/hdr.md
# Verify a truncated component-RLE stream is rejected.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "loader/0132_loader_builtin_hdr_new_rle_truncated_reject" || {
    echo "not ok 1 - HDR component RLE rejects truncation"
    exit 0
}

echo "ok 1 - HDR component RLE rejects truncation"
exit 0
