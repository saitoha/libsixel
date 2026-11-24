#!/bin/sh
# Policy: docs/encoder/palette-application/lookup-policy/mahalanobis.md
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" mahalanobis/gpu_live_plan_isolation || {
    printf '\n%s\n' "not ok 1 - mahalanobis gpu live plan isolation"
    exit 0
}
printf '\n%s\n' "ok 1 - mahalanobis gpu live plan isolation"
exit 0
