#!/bin/sh

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" icc/0099_mpet_definition_absolute_fallback_forward || {
    echo "not ok 1 - absolute ICC definition retries slot zero forward"
    exit 0
}

echo "ok 1 - absolute ICC definition retries slot zero forward"
exit 0
