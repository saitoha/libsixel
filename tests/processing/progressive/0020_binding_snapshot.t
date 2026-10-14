#!/bin/sh
# Verify binding snapshot.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "progressive/0020_binding_snapshot" || {
    echo "not ok 1 - binding snapshot"
    exit 0
}
echo "ok 1 - binding snapshot"
exit 0
