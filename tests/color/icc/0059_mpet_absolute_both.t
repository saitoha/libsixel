#!/bin/sh

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" icc/0059_mpet_absolute_both || {
    printf '\n%s\n' "not ok 1 - absolute mpet slots bypass both media white corrections"
    exit 0
}

printf '\n%s\n' "ok 1 - absolute mpet slots bypass both media white corrections"
exit 0
