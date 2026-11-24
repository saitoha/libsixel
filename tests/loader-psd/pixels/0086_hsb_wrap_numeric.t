#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Verify hsb wrap numeric.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/psd/0072_hsb_wrap_numeric 1>&2 || {
    printf '\n%s\n' "not ok 1 - hsb wrap numeric"
    exit 0
}
printf '\n%s\n' "ok 1 - hsb wrap numeric"
exit 0
