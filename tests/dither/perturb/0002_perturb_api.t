#!/bin/sh
# Verify perturb api through the internal C contract.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "perturb/0002_perturb_api" || {
    printf '\n%s\n' "not ok 1 - 0002_perturb_api"
    exit 0
}
printf '\n%s\n' "ok 1 - 0002_perturb_api"
exit 0
