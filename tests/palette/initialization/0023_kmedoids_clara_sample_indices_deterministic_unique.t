#!/bin/sh
# TAP test: CLARA sample indices are unique and deterministic for fixed seed.

set -eux


echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "palette/0002_kmedoids_constraints" clara-sample-indices >/dev/null || {
    echo "not ok" 1 - "kmedoids CLARA sample index validation failed"
    exit 0
}

echo "ok" 1 - "kmedoids CLARA sample indices are deterministic and unique"
exit 0
