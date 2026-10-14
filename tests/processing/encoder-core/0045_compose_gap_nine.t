#!/bin/sh
# Verify compose gap nine.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "encoder-core/0045_compose_gap_nine" || {
    echo "not ok 1 - compose gap nine"
    exit 0
}

echo "ok 1 - compose gap nine"
exit 0
