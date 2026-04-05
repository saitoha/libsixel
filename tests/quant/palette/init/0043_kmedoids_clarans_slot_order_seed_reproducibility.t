#!/bin/sh
# TAP test: CLARANS slot-order path remains deterministic for fixed seed.

set -eux


echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "palette/0002_kmedoids_constraints" clarans-slot-order-seed-reproducibility \
    >/dev/null || {
    echo "not ok" 1 - "kmedoids CLARANS slot order seed reproducibility failed"
    exit 0
}

echo "ok" 1 - "kmedoids CLARANS slot order seed reproducibility passed"
exit 0
