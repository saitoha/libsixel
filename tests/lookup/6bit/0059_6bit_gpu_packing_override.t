#!/bin/sh
# Policy: docs/encoder/palette-application/lookup-policy/6bit.md
# Verify 6bit GPU packing override.
set -eux
echo "1..1"
set -v
status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" --require-gpu-palette \
    6bit/gpu_packing_override || status=$?
test "$status" -ne 77 || {
    echo "ok 1 # SKIP GPU palette backend unavailable"
    exit 0
}
test "$status" -eq 0 || {
    echo "not ok 1 - 6bit GPU packing override"
    exit 0
}
echo "ok 1 - 6bit GPU packing override"
exit 0
