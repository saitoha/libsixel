#!/bin/sh
# None performs no snapshot allocation.
# Policy: docs/encoder/palette-application/clamp-policy.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    dither/clamp/0044_snapshot_allocation 2 || {
    echo "not ok 1 - none_no_snapshot"
    exit 0
}
echo "ok 1 - none_no_snapshot"
exit 0
