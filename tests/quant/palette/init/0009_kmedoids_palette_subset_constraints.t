#!/bin/sh
# TAP test: k-medoids keeps palette colors in the input color set.

set -eux


echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "palette/0002_kmedoids_constraints" >/dev/null || {
    echo "not ok" 1 - "kmedoids palette subset constraint failed"
    exit 0
}

echo "ok" 1 - "kmedoids palette subset constraint passed"
exit 0
