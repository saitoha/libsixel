#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Verify sof3 precision12 against every original sample.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/jpeg/0180_sof3_precision12_numeric 1>&2 || {
    echo "not ok 1 - sof3_precision12 numeric reconstruction"
    exit 0
}
echo "ok 1 - sof3_precision12 numeric reconstruction"
exit 0
