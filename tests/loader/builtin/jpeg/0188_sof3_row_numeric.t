#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Verify sof3 row against every original sample.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/jpeg/0188_sof3_row_numeric 1>&2 || {
    echo "not ok 1 - sof3_row numeric reconstruction"
    exit 0
}
echo "ok 1 - sof3_row numeric reconstruction"
exit 0
