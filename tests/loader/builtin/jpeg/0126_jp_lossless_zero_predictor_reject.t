#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Reject lossless zero predictor.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "loader/jpeg/0126_jp_lossless_zero_predictor_reject" 1>&2 || {
    echo "not ok 1 - Reject lossless zero predictor"
    exit 0
}
echo "ok 1 - Reject lossless zero predictor"
exit 0
