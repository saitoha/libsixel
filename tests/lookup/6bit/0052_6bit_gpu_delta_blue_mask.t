#!/bin/sh
# Policy: docs/encoder/palette-application/lookup-policy/6bit.md
# Verify 6bit GPU delta blue mask.
set -eux
echo "1..1"
set -v
status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" --require-gpu-palette-direct \
    6bit/gpu_delta_blue_mask || status=$?
test "$status" -ne 77 || {
    echo "ok 1 # SKIP GPU palette backend unavailable"
    exit 0
}
test "$status" -eq 0 || {
    echo "not ok 1 - 6bit GPU delta blue mask"
    exit 0
}
echo "ok 1 - 6bit GPU delta blue mask"
exit 0
