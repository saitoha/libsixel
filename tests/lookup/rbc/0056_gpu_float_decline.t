#!/bin/sh
# Policy: docs/encoder/palette-application/lookup-policy/rbc.md
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" rbc/gpu_float_decline || {
    printf '\n%s\n' "not ok 1 - rbc gpu float decline"
    exit 0
}
printf '\n%s\n' "ok 1 - rbc gpu float decline"
exit 0
