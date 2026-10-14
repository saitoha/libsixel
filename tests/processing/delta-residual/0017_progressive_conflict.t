#!/bin/sh
# Verify progressive conflict.
# Policy: docs/encoder/output/delta-encoding.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" delta-residual/0017_progressive_conflict || {
    echo "not ok 1 - progressive conflict"
    exit 0
}
echo "ok 1 - progressive conflict"
exit 0
