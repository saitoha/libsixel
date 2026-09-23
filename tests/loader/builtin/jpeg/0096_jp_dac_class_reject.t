#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Reject dac class.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "loader/jpeg/0096_jp_dac_class_reject" 1>&2 || {
    echo "not ok 1 - Reject dac class"
    exit 0
}
echo "ok 1 - Reject dac class"
exit 0
