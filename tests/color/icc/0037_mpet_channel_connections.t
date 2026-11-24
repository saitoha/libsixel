#!/bin/sh

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" icc/0037_mpet_channel_connections || {
    printf '\n%s\n' "not ok 1 - mpet rejects mismatched channel connections"
    exit 0
}

printf '\n%s\n' "ok 1 - mpet rejects mismatched channel connections"
exit 0
