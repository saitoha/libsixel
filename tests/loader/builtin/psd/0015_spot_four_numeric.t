#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Verify spot four numeric.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/psd/0015_spot_four_numeric 1>&2 || {
    echo "not ok 1 - spot four numeric"
    exit 0
}
echo "ok 1 - spot four numeric"
exit 0
