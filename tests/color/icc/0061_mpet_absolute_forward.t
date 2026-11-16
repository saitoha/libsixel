#!/bin/sh

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" icc/0061_mpet_absolute_forward || {
    echo "not ok 1 - D2B3 reaches a relative destination with one white correction"
    exit 0
}

echo "ok 1 - D2B3 reaches a relative destination with one white correction"
exit 0
