#!/bin/sh
# Verify history revisions.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "progressive/0002_history_revisions" || {
    printf '\n%s\n' "not ok 1 - history revisions"
    exit 0
}
printf '\n%s\n' "ok 1 - history revisions"
exit 0
