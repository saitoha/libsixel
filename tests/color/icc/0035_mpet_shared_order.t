#!/bin/sh

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" icc/0035_mpet_shared_order || {
    echo "not ok 1 - mpet keeps element order and shares an owned payload"
    exit 0
}

echo "ok 1 - mpet keeps element order and shares an owned payload"
exit 0
