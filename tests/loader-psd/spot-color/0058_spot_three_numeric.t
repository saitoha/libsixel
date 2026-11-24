#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Verify spot three numeric.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/psd/0014_spot_three_numeric 1>&2 || {
    printf '\n%s\n' "not ok 1 - spot three numeric"
    exit 0
}
printf '\n%s\n' "ok 1 - spot three numeric"
exit 0
