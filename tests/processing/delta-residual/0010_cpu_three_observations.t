#!/bin/sh
# Verify cpu three observations.
# Policy: docs/encoder/output/delta-encoding.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" delta-residual/0010_cpu_three_observations || {
    echo "not ok 1 - cpu three observations"
    exit 0
}
echo "ok 1 - cpu three observations"
exit 0
