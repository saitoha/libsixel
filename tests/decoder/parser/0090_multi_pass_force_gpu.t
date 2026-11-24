#!/bin/sh
# Verify force gpu.
# Policy: docs/decoder/multi-pass.md

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "decoder/0080_multi_pass_force_gpu" || {
    printf '\n%s\n' "not ok 1 - force gpu"
    exit 0
}

printf '\n%s\n' "ok 1 - force gpu"
exit 0
