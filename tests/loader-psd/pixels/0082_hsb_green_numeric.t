#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Verify hsb green numeric.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/psd/0068_hsb_green_numeric 1>&2 || {
    echo "not ok 1 - hsb green numeric"
    exit 0
}
echo "ok 1 - hsb green numeric"
exit 0
