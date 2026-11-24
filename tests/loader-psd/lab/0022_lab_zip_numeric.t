#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Verify lab zip numeric.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/psd/0191_lab_zip_numeric 1>&2 || {
    printf '\n%s\n' "not ok 1 - lab zip numeric"
    exit 0
}
printf '\n%s\n' "ok 1 - lab zip numeric"
exit 0
