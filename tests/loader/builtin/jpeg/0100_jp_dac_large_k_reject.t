#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Reject dac large k.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "loader/jpeg/0100_jp_dac_large_k_reject" 1>&2 || {
    echo "not ok 1 - Reject dac large k"
    exit 0
}
echo "ok 1 - Reject dac large k"
exit 0
