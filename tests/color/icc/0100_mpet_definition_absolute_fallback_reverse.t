#!/bin/sh

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" icc/0100_mpet_definition_absolute_fallback_reverse || {
    echo "not ok 1 - absolute ICC definition retries slot zero in reverse"
    exit 0
}

echo "ok 1 - absolute ICC definition retries slot zero in reverse"
exit 0
