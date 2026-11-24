#!/bin/sh
# TAP test: CLARANS residual eval-order remains deterministic.

set -eux


echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "palette/0002_kmedoids_constraints" \
    clarans-residual-eval-order-determinism \
    >/dev/null || {
    printf '\n%s\n' "not ok 1 - kmedoids CLARANS residual eval-order determinism failed"
    exit 0
}

printf '\n%s\n' "ok 1 - kmedoids CLARANS residual eval-order determinism passed"
exit 0
