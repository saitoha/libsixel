#!/bin/sh
# Soft owns one exact RGB float32 snapshot.
# Policy: docs/encoder/palette-application/clamp-policy.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    dither/clamp/0047_snapshot_allocation 0 || {
    echo "not ok 1 - soft_snapshot"
    exit 0
}
echo "ok 1 - soft_snapshot"
exit 0
