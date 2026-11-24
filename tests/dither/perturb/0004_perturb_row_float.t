#!/bin/sh
# Verify perturb row float through the internal C contract.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "perturb/0004_perturb_row_float" || {
    printf '\n%s\n' "not ok 1 - 0004_perturb_row_float"
    exit 0
}
printf '\n%s\n' "ok 1 - 0004_perturb_row_float"
exit 0
