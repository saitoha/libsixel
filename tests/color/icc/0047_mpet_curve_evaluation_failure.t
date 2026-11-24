#!/bin/sh

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" icc/0047_mpet_curve_evaluation_failure || {
    printf '\n%s\n' "not ok 1 - mpet rejects a nonfinite formula result"
    exit 0
}

printf '\n%s\n' "ok 1 - mpet rejects a nonfinite formula result"
exit 0
