#!/bin/sh
# Verify perturb weights through the internal C contract.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "perturb/0001_perturb_weights" || {
    printf '\n%s\n' "not ok 1 - 0001_perturb_weights"
    exit 0
}
printf '\n%s\n' "ok 1 - 0001_perturb_weights"
exit 0
