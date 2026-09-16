#!/bin/sh
# Verify float K-center keeps its legacy half-open hard-grid palette.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "palette/0043_kcenter_float_legacy_grid" || {
    echo "not ok 1 - K-center float hard-grid regression"
    exit 0
}

echo "ok 1 - K-center float hard-grid regression"
exit 0
