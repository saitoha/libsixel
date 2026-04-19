#!/bin/sh
# TAP test: swap_update incremental/full should be deterministic with same seed.

set -eux


echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "palette/0003_kcenter_constraints" swap-update-consistency >/dev/null || {
    echo "not ok" 1 - "kcenter swap_update consistency check failed"
    exit 0
}

echo "ok" 1 - "kcenter swap_update consistency check passed"
exit 0
