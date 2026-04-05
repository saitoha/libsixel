#!/bin/sh
# TAP test: CLARANS lazy slot-order path keeps seed reproducibility.

set -eux


echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "palette/0002_kmedoids_constraints" \
    clarans-slot-order-lazy-seed-reproducibility \
    >/dev/null || {
    echo "not ok" 1 - "kmedoids CLARANS lazy slot-order seed failed"
    exit 0
}

echo "ok" 1 - "kmedoids CLARANS lazy slot-order seed passed"
exit 0
