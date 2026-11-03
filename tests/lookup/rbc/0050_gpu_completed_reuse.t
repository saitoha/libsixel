#!/bin/sh
# Policy: docs/encoder/palette-application/lookup-policy/rbc.md
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" rbc/gpu_completed_reuse || {
    echo "not ok 1 - rbc gpu completed reuse"
    exit 0
}
echo "ok 1 - rbc gpu completed reuse"
exit 0
