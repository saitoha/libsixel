#!/bin/sh
# Verify cpu a observations.
# Policy: docs/encoder/output/delta-encoding.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" delta-residual/0023_cpu_a_observations || {
    echo "not ok 1 - cpu a observations"
    exit 0
}
echo "ok 1 - cpu a observations"
exit 0
