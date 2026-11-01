#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Verify spot nonfinite reject.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/psd/0034_spot_nonfinite_reject 1>&2 || {
    echo "not ok 1 - spot nonfinite reject"
    exit 0
}
echo "ok 1 - spot nonfinite reject"
exit 0
