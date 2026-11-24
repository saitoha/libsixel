#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Policy: docs/loader/builtin/hdr.md
# Verify a zero-length component-RLE packet is rejected.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "loader/0130_loader_builtin_hdr_new_rle_zero_count_reject" || {
    printf '\n%s\n' "not ok 1 - HDR component RLE rejects a zero count"
    exit 0
}

printf '\n%s\n' "ok 1 - HDR component RLE rejects a zero count"
exit 0
