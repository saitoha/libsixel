#!/bin/sh
# Verify metric boundaries.
# Policy: docs/encoder/output/delta-encoding.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" delta-residual/0006_metric_boundaries || {
    printf '\n%s\n' "not ok 1 - metric boundaries"
    exit 0
}
printf '\n%s\n' "ok 1 - metric boundaries"
exit 0
