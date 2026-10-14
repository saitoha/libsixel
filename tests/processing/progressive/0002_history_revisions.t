#!/bin/sh
# Verify history revisions.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "progressive/0002_history_revisions" || {
    echo "not ok 1 - history revisions"
    exit 0
}
echo "ok 1 - history revisions"
exit 0
