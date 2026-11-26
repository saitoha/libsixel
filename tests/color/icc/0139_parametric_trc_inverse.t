#!/bin/sh

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" icc/0139_parametric_trc_inverse || {
    printf '\n%s\n' "not ok 1 - parametric trc inverse preserves device boundaries"
    exit 0
}

printf '\n%s\n' "ok 1 - parametric trc inverse preserves device boundaries"
exit 0
