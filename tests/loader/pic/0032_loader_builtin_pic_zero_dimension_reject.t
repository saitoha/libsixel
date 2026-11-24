#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Policy: docs/loader/builtin/pic.md
# Verify zero-width and zero-height PIC canvases are rejected.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "loader/0120_loader_builtin_pic_zero_dimension_reject" || {
    printf '\n%s\n' "not ok 1 - PIC zero dimensions are rejected"
    exit 0
}

printf '\n%s\n' "ok 1 - PIC zero dimensions are rejected"
exit 0
