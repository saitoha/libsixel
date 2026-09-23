#!/bin/sh
# Policy: docs/loader/builtin/jpeg.md
# Test-plan: docs/testing/builtin-loader-coverage.md
# Verify 8-bit CMYK source and fallback allocation cleanup.
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" bjicc/0052_bj_ink8_allocation 1>&2 || {
    echo "not ok 1 - CMYK8 allocation cleanup"
    exit 0
}
echo "ok 1 - CMYK8 allocation cleanup"
exit 0
