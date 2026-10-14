#!/bin/sh
# Verify cpu x observations.
# Policy: docs/encoder/output/delta-encoding.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" delta-residual/0024_cpu_x_observations || {
    echo "not ok 1 - cpu x observations"
    exit 0
}
echo "ok 1 - cpu x observations"
exit 0
