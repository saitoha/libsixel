#!/bin/sh
# Verify compose allocation recovery.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "encoder-core/0049_compose_allocation_recovery" || {
    echo "not ok 1 - compose allocation recovery"
    exit 0
}

echo "ok 1 - compose allocation recovery"
exit 0
