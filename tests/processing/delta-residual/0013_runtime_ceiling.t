#!/bin/sh
# Verify runtime ceiling.
# Policy: docs/encoder/output/delta-encoding.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" delta-residual/0013_runtime_ceiling || {
    echo "not ok 1 - runtime ceiling"
    exit 0
}
echo "ok 1 - runtime ceiling"
exit 0
