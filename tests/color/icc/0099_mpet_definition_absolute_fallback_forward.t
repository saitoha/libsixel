#!/bin/sh

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" icc/0099_mpet_definition_absolute_fallback_forward || {
    printf '\n%s\n' "not ok 1 - absolute ICC definition retries slot zero forward"
    exit 0
}

printf '\n%s\n' "ok 1 - absolute ICC definition retries slot zero forward"
exit 0
