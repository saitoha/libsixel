#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Verify spot one numeric.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/psd/0012_spot_one_numeric 1>&2 || {
    printf '\n%s\n' "not ok 1 - spot one numeric"
    exit 0
}
printf '\n%s\n' "ok 1 - spot one numeric"
exit 0
