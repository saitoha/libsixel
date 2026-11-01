#!/bin/sh
# Verify float FHEDT retention and allocation cleanup.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    planner/pipeline/0031_palette_snapshot_fhedt_float || {
    echo "not ok 1 - float FHEDT retention and allocation cleanup"
    exit 0
}
echo "ok 1 - float FHEDT retention and allocation cleanup"
exit 0
