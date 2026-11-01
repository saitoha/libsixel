#!/bin/sh
# Policy: docs/loader/builtin/jpeg.md
# Test-plan: docs/testing/builtin-loader-coverage.md
# Verify sof3 dnl zero against every original sample.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/jpeg/0189_sof3_dnl_zero_numeric 1>&2 || {
    echo "not ok 1 - sof3_dnl_zero numeric reconstruction"
    exit 0
}
echo "ok 1 - sof3_dnl_zero numeric reconstruction"
exit 0
