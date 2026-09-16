#!/bin/sh
# TAP test: swap radius should not increase with additional iterations.

set -eux


echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "palette/0003_kcenter_constraints" swap-monotonic-radius >/dev/null || {
    echo "not ok" 1 - "kcenter swap monotonic-radius check failed"
    exit 0
}

echo "ok" 1 - "kcenter swap monotonic-radius check passed"
exit 0
