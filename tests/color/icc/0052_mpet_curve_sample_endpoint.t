#!/bin/sh

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" icc/0052_mpet_curve_sample_endpoint || {
    printf '\n%s\n' "not ok 1 - mpet interpolation preserves distance to the upper breakpoint"
    exit 0
}

printf '\n%s\n' "ok 1 - mpet interpolation preserves distance to the upper breakpoint"
exit 0
