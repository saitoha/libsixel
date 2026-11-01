#!/bin/sh
# Policy: docs/loader/builtin/jpeg.md
# Test-plan: docs/testing/builtin-loader-coverage.md
# Verify 16-bit CMYK source and fallback allocation cleanup.
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" bjicc/0053_bj_ink16_allocation 1>&2 || {
    echo "not ok 1 - CMYK16 allocation cleanup"
    exit 0
}
echo "ok 1 - CMYK16 allocation cleanup"
exit 0
