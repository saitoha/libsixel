#!/bin/sh
# Verify policy age reset.
# Policy: docs/encoder/output/delta-encoding.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" delta-residual/0005_policy_age_reset || {
    echo "not ok 1 - policy age reset"
    exit 0
}
echo "ok 1 - policy age reset"
exit 0
