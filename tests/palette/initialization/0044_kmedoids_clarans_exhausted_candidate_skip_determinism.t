#!/bin/sh
# TAP test: CLARANS exhausted-candidate skip path remains deterministic.

set -eux


echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "palette/0002_kmedoids_constraints" \
    clarans-exhausted-candidate-skip-determinism \
    >/dev/null || {
    echo "not ok" 1 - "kmedoids CLARANS exhausted candidate skip failed"
    exit 0
}

echo "ok" 1 - "kmedoids CLARANS exhausted candidate skip passed"
exit 0
