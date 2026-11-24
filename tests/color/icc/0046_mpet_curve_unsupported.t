#!/bin/sh

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" icc/0046_mpet_curve_unsupported || {
    printf '\n%s\n' "not ok 1 - mpet discards a curve set with an unsupported segment"
    exit 0
}

printf '\n%s\n' "ok 1 - mpet discards a curve set with an unsupported segment"
exit 0
