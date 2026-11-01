#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Verify hsb gray numeric.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/psd/0073_hsb_gray_numeric 1>&2 || {
    echo "not ok 1 - hsb gray numeric"
    exit 0
}
echo "ok 1 - hsb gray numeric"
exit 0
