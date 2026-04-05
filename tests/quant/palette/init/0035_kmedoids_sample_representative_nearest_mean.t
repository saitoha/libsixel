#!/bin/sh
# TAP test: selected bin representative is nearest observed color to bin mean.

set -eux


echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "palette/0002_kmedoids_constraints" sample-representative-nearest-mean \
    >/dev/null || {
    echo "not ok" 1 - "kmedoids sample representative nearest-mean check failed"
    exit 0
}

echo "ok" 1 - "kmedoids sample representative nearest-mean check passed"
exit 0
