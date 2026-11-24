#!/bin/sh

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" icc/0045_mpet_curve_structure || {
    printf '\n%s\n' "not ok 1 - mpet rejects malformed curve data"
    exit 0
}

printf '\n%s\n' "ok 1 - mpet rejects malformed curve data"
exit 0
