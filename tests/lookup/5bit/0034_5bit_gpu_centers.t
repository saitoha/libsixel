#!/bin/sh
# Policy: docs/encoder/palette-application/lookup-policy/5bit.md
# Verify 5bit GPU centers.
set -eux
echo "1..1"
set -v
status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" --require-gpu-palette-direct \
    5bit/gpu_centers || status=$?
test "$status" -ne 77 || {
    printf '\n%s\n' "ok 1 # SKIP GPU palette backend unavailable"
    exit 0
}
test "$status" -eq 0 || {
    printf '\n%s\n' "not ok 1 - 5bit GPU centers"
    exit 0
}
printf '\n%s\n' "ok 1 - 5bit GPU centers"
exit 0
