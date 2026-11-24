#!/bin/sh

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" icc/0062_mpet_pcs_lab_forward || {
    printf '\n%s\n' "not ok 1 - D2B emits physical PCS Lab"
    exit 0
}

printf '\n%s\n' "ok 1 - D2B emits physical PCS Lab"
exit 0
