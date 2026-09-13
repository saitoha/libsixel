#!/bin/sh
# Verify perturb api through the internal C contract.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "perturb/0002_perturb_api" || {
    echo "not ok 1 - 0002_perturb_api"
    exit 0
}
echo "ok 1 - 0002_perturb_api"
exit 0
