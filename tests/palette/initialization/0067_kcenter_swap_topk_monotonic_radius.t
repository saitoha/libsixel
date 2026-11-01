#!/bin/sh
# TAP test: swap_topk path should keep non-increasing radius across iterations.

set -eux


echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "palette/0003_kcenter_constraints" swap-topk-monotonic-radius >/dev/null || {
    echo "not ok" 1 - "kcenter swap_topk monotonic-radius check failed"
    exit 0
}

echo "ok" 1 - "kcenter swap_topk monotonic-radius check passed"
exit 0
