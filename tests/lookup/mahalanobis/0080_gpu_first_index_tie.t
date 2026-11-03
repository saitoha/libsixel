#!/bin/sh
# Policy: docs/encoder/palette-application/lookup-policy/mahalanobis.md
set -eux
echo "1..1"
set -v

status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" --require-gpu-palette \
    mahalanobis/gpu_first_index_tie || status=$?
test "$status" -ne 77 || {
    echo "ok 1 # SKIP GPU palette backend unavailable"
    exit 0
}
test "$status" -eq 0 || {
    echo "not ok 1 - mahalanobis gpu first index tie"
    exit 0
}
echo "ok 1 - mahalanobis gpu first index tie"
exit 0
