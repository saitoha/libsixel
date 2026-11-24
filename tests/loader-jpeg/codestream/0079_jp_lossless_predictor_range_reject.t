#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Reject lossless predictor range.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "loader/jpeg/0129_jp_lossless_predictor_range_reject" 1>&2 || {
    printf '\n%s\n' "not ok 1 - Reject lossless predictor range"
    exit 0
}
printf '\n%s\n' "ok 1 - Reject lossless predictor range"
exit 0
