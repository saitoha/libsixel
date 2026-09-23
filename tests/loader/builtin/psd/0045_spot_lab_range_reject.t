#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Verify spot lab range reject.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/psd/0045_spot_lab_range_reject 1>&2 || {
    echo "not ok 1 - spot lab range reject"
    exit 0
}
echo "ok 1 - spot lab range reject"
exit 0
