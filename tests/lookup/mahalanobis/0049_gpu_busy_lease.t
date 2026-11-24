#!/bin/sh
# Policy: docs/encoder/palette-application/lookup-policy/mahalanobis.md
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" mahalanobis/gpu_busy_lease || {
    printf '\n%s\n' "not ok 1 - mahalanobis gpu busy lease"
    exit 0
}
printf '\n%s\n' "ok 1 - mahalanobis gpu busy lease"
exit 0
