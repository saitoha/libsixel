#!/bin/sh
# Verify the weighted-point-set ownership and metadata contract.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "palette/0031_weighted_point_set" || {
    echo "not ok 1 - weighted point set contract"
    exit 0
}

echo "ok 1 - weighted point set contract"
exit 0
