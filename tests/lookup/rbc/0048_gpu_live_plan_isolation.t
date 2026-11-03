#!/bin/sh
# Policy: docs/encoder/palette-application/lookup-policy/rbc.md
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" rbc/gpu_live_plan_isolation || {
    echo "not ok 1 - rbc gpu live plan isolation"
    exit 0
}
echo "ok 1 - rbc gpu live plan isolation"
exit 0
