#!/bin/sh

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" icc/0060_mpet_absolute_reverse || {
    echo "not ok 1 - B2D3 receives absolute PCS from a relative source"
    exit 0
}

echo "ok 1 - B2D3 receives absolute PCS from a relative source"
exit 0
