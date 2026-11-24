#!/bin/sh

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" icc/0063_mpet_pcs_lab_reverse || {
    printf '\n%s\n' "not ok 1 - B2D receives signed PCS Lab"
    exit 0
}

printf '\n%s\n' "ok 1 - B2D receives signed PCS Lab"
exit 0
