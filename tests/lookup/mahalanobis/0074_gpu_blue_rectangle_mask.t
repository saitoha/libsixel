#!/bin/sh
# Policy: docs/encoder/palette-application/lookup-policy/mahalanobis.md
set -eux
echo "1..1"
set -v

status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" --require-gpu-lookup mahalanobis mahalanobis/gpu_blue_rectangle_mask || status=$?
test "$status" -ne 77 || {
    echo "ok 1 # SKIP GPU mahalanobis program unavailable"
    exit 0
}
test "$status" -eq 0 || {
    echo "not ok 1 - mahalanobis gpu blue rectangle mask"
    exit 0
}
echo "ok 1 - mahalanobis gpu blue rectangle mask"
exit 0
