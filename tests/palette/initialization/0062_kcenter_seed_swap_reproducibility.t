#!/bin/sh
# TAP test: k-center seed reproducibility for swap.

set -eux


echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "palette/0003_kcenter_constraints" seed-swap >/dev/null || {
    echo "not ok" 1 - "kcenter seed reproducibility failed for swap"
    exit 0
}

echo "ok" 1 - "kcenter seed reproducibility passed for swap"
exit 0
