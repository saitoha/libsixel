#!/bin/sh
# Verify history reset.
# Policy: docs/encoder/output/delta-encoding.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" delta-residual/0014_history_reset || {
    echo "not ok 1 - history reset"
    exit 0
}
echo "ok 1 - history reset"
exit 0
