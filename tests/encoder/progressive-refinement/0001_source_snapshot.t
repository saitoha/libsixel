#!/bin/sh
# Verify source snapshot.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "progressive/0001_source_snapshot" || {
    printf '\n%s\n' "not ok 1 - source snapshot"
    exit 0
}
printf '\n%s\n' "ok 1 - source snapshot"
exit 0
