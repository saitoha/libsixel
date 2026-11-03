#!/bin/sh
# Policy: docs/encoder/palette-application/lookup-policy/rbc.md
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" rbc/gpu_binding_lifetime || {
    echo "not ok 1 - rbc gpu binding lifetime"
    exit 0
}
echo "ok 1 - rbc gpu binding lifetime"
exit 0
