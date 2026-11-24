#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Verify spot zip numeric.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/psd/0021_spot_zip_numeric 1>&2 || {
    printf '\n%s\n' "not ok 1 - spot zip numeric"
    exit 0
}
printf '\n%s\n' "ok 1 - spot zip numeric"
exit 0
