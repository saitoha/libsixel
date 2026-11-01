#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Verify hsb cyan numeric.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/psd/0069_hsb_cyan_numeric 1>&2 || {
    echo "not ok 1 - hsb cyan numeric"
    exit 0
}
echo "ok 1 - hsb cyan numeric"
exit 0
