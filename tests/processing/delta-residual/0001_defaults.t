#!/bin/sh
# Verify defaults.
# Policy: docs/encoder/output/delta-encoding.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" delta-residual/0001_defaults || {
    echo "not ok 1 - defaults"
    exit 0
}
echo "ok 1 - defaults"
exit 0
