#!/bin/sh

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" icc/0037_mpet_channel_connections || {
    echo "not ok 1 - mpet rejects mismatched channel connections"
    exit 0
}

echo "ok 1 - mpet rejects mismatched channel connections"
exit 0
