#!/bin/sh
# Verify classify allocation recovery.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "encoder-core/0053_classify_allocation_recovery" || {
    echo "not ok 1 - classify allocation recovery"
    exit 0
}

echo "ok 1 - classify allocation recovery"
exit 0
