#!/bin/sh

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" icc/0044_mpet_curve_samples || {
    printf '\n%s\n' "not ok 1 - mpet interpolates samples and selects breakpoint intervals"
    exit 0
}

printf '\n%s\n' "ok 1 - mpet interpolates samples and selects breakpoint intervals"
exit 0
