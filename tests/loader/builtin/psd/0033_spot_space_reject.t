#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Verify spot space reject.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/psd/0033_spot_space_reject 1>&2 || {
    echo "not ok 1 - spot space reject"
    exit 0
}
echo "ok 1 - spot space reject"
exit 0
