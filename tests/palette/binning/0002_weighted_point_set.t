#!/bin/sh
# Verify the weighted-point-set ownership and metadata contract.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "palette/0031_weighted_point_set" || {
    printf '\n%s\n' "not ok 1 - weighted point set contract"
    exit 0
}

printf '\n%s\n' "ok 1 - weighted point set contract"
exit 0
