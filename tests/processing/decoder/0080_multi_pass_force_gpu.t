#!/bin/sh
# Verify force gpu.
# Policy: docs/decoder/multi-pass.md

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "decoder/0080_multi_pass_force_gpu" || {
    echo "not ok 1 - force gpu"
    exit 0
}

echo "ok 1 - force gpu"
exit 0
