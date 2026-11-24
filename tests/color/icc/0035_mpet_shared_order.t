#!/bin/sh

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" icc/0035_mpet_shared_order || {
    printf '\n%s\n' "not ok 1 - mpet keeps element order and shares an owned payload"
    exit 0
}

printf '\n%s\n' "ok 1 - mpet keeps element order and shares an owned payload"
exit 0
