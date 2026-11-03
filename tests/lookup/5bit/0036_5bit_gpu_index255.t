#!/bin/sh
# Policy: docs/encoder/palette-application/lookup-policy/5bit.md
# Verify 5bit GPU index255.
set -eux
echo "1..1"
set -v
status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" --require-gpu-palette \
    5bit/gpu_index255 || status=$?
test "$status" -ne 77 || {
    echo "ok 1 # SKIP GPU palette backend unavailable"
    exit 0
}
test "$status" -eq 0 || {
    echo "not ok 1 - 5bit GPU index255"
    exit 0
}
echo "ok 1 - 5bit GPU index255"
exit 0
