#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Verify hsb blue numeric.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/psd/0070_hsb_blue_numeric 1>&2 || {
    echo "not ok 1 - hsb blue numeric"
    exit 0
}
echo "ok 1 - hsb blue numeric"
exit 0
