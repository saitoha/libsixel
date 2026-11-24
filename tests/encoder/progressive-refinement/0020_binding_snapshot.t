#!/bin/sh
# Verify binding snapshot.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "progressive/0020_binding_snapshot" || {
    printf '\n%s\n' "not ok 1 - binding snapshot"
    exit 0
}
printf '\n%s\n' "ok 1 - binding snapshot"
exit 0
