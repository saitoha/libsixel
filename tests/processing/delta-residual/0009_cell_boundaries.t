#!/bin/sh
# Verify cell boundaries.
# Policy: docs/encoder/output/delta-encoding.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" delta-residual/0009_cell_boundaries || {
    echo "not ok 1 - cell boundaries"
    exit 0
}
echo "ok 1 - cell boundaries"
exit 0
