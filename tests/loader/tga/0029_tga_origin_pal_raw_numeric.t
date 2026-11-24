#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Policy: docs/loader/builtin/tga.md
# Rebase raw eight-bit file indices to exact zero-based PAL8 slots.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "loader/0229_tga_origin_pal_raw_numeric" 1>&2 || {
    printf '\n%s\n' "not ok 1 - Rebase raw eight-bit file indices to exact zero-based PAL8 slots"
    exit 0
}
printf '\n%s\n' "ok 1 - Rebase raw eight-bit file indices to exact zero-based PAL8 slots"
exit 0
