#!/bin/sh

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" icc/0053_status_mpet_slot3_supported || {
    printf '\n%s\n' "not ok 1 - profile open accepts absolute mpet slots"
    exit 0
}

printf '\n%s\n' "ok 1 - profile open accepts absolute mpet slots"
exit 0
