#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Verify hsb mixed numeric.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/psd/0074_hsb_mixed_numeric 1>&2 || {
    echo "not ok 1 - hsb mixed numeric"
    exit 0
}
echo "ok 1 - hsb mixed numeric"
exit 0
