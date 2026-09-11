#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Policy: docs/loader/builtin/hdr.md
# Verify modern component RLE emits exact linear float samples.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "loader/0105_loader_builtin_hdr_new_rle_numeric" || {
    echo "not ok 1 - HDR component RLE exact float samples"
    exit 0
}

echo "ok 1 - HDR component RLE exact float samples"
exit 0
