#!/bin/sh
# Policy: docs/encoder/palette-application/lookup-policy/mahalanobis.md
set -eux
echo "1..1"
set -v

status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" --require-gpu-palette-direct mahalanobis/gpu_exact_1_0_64 || status=$?
test "$status" -ne 77 || {
    echo "ok 1 # SKIP GPU palette backend unavailable"
    exit 0
}
test "$status" -eq 0 || {
    echo "not ok 1 - mahalanobis gpu exact 1 0 64"
    exit 0
}
echo "ok 1 - mahalanobis gpu exact 1 0 64"
exit 0
