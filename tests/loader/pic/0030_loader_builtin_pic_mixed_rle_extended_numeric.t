#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Policy: docs/loader/builtin/pic.md
# Verify PIC mixed RLE expands the 16-bit extended repeat count exactly.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "loader/0099_loader_builtin_pic_mixed_rle_extended_numeric" || {
    printf '\n%s\n' "not ok 1 - PIC mixed RLE exact extended repeat count"
    exit 0
}

printf '\n%s\n' "ok 1 - PIC mixed RLE exact extended repeat count"
exit 0
