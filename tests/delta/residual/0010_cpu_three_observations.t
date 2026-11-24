#!/bin/sh
# Verify cpu three observations.
# Policy: docs/encoder/output/delta-encoding.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" delta-residual/0010_cpu_three_observations || {
    printf '\n%s\n' "not ok 1 - cpu three observations"
    exit 0
}
printf '\n%s\n' "ok 1 - cpu three observations"
exit 0
