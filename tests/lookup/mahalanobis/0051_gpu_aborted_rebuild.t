#!/bin/sh
# Policy: docs/encoder/palette-application/lookup-policy/mahalanobis.md
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" mahalanobis/gpu_aborted_rebuild || {
    echo "not ok 1 - mahalanobis gpu aborted rebuild"
    exit 0
}
echo "ok 1 - mahalanobis gpu aborted rebuild"
exit 0
