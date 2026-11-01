#!/bin/sh
# Verify byte FHEDT retention and allocation cleanup.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    planner/pipeline/0030_palette_snapshot_fhedt_bytes || {
    echo "not ok 1 - byte FHEDT retention and allocation cleanup"
    exit 0
}
echo "ok 1 - byte FHEDT retention and allocation cleanup"
exit 0
