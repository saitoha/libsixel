#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Verify spot lab numeric.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/psd/0041_spot_lab_numeric 1>&2 || {
    echo "not ok 1 - spot lab numeric"
    exit 0
}
echo "ok 1 - spot lab numeric"
exit 0
