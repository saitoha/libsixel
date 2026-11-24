#!/bin/sh

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" icc/0094_mpet_legacy_slot_pair || {
    printf '\n%s\n' "not ok 1 - A dormant mpet leaves legacy endpoint slot ordering intact"
    exit 0
}

printf '\n%s\n' "ok 1 - A dormant mpet leaves legacy endpoint slot ordering intact"
exit 0
