#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Policy: docs/loader/builtin/hdr.md
# Verify a component-RLE run cannot exceed the scanline.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "loader/0131_loader_builtin_hdr_new_rle_overrun_reject" || {
    printf '\n%s\n' "not ok 1 - HDR component RLE rejects a scanline overrun"
    exit 0
}

printf '\n%s\n' "ok 1 - HDR component RLE rejects a scanline overrun"
exit 0
