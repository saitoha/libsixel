#!/bin/sh
# Verify native snap: none identity.
# Policy: docs/encoder/palette-construction/snap-policy/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/snap_float/0001_snap_contract unit 0 0 1 2 || {
    echo "not ok 1 - none identity"
    exit 0
}

echo "ok 1 - none identity"
exit 0
