#!/bin/sh
# Verify context allocation failure.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "progressive/0023_context_allocation_failure" || {
    printf '\n%s\n' "not ok 1 - context allocation failure"
    exit 0
}
printf '\n%s\n' "ok 1 - context allocation failure"
exit 0
