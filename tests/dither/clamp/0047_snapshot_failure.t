#!/bin/sh
# Snapshot allocation failure cleans up and recovers.
# Policy: docs/encoder/palette-application/clamp-policy.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    dither/clamp/0044_snapshot_allocation 3 || {
    echo "not ok 1 - snapshot_failure"
    exit 0
}
echo "ok 1 - snapshot_failure"
exit 0
