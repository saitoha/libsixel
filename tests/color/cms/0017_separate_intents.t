#!/bin/sh
# Verify separate intents.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "cms/0011_separate_intents" || {
    printf '\n%s\n' "not ok 1 - separate intents"
    exit 0
}

printf '\n%s\n' "ok 1 - separate intents"
exit 0
