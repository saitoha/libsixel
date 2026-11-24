#!/bin/sh
# Policy: docs/encoder/palette-application/lookup-policy/mahalanobis.md
set -eux
echo "1..1"
set -v

status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" --require-gpu-lookup mahalanobis \
    mahalanobis/gpu_palette_rebuild || status=$?
test "$status" -ne 77 || {
    printf '\n%s\n' "ok 1 # SKIP GPU mahalanobis program unavailable"
    exit 0
}
test "$status" -eq 0 || {
    printf '\n%s\n' "not ok 1 - mahalanobis gpu palette rebuild"
    exit 0
}
printf '\n%s\n' "ok 1 - mahalanobis gpu palette rebuild"
exit 0
