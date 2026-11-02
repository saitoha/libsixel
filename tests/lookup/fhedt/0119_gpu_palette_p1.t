#!/bin/sh
# Policy: docs/encoder/palette-application/lookup-policy/fhedt.md
# Verify gpu palette p1.
set -eux
echo "1..1"
set -v
status=0
SIXEL_LOOKUP_FHEDT_RESOLUTION=64 SIXEL_LOOKUP_FHEDT_REFINE=1 SIXEL_LOOKUP_FHEDT_USE_DIST2=0 SIXEL_LOOKUP_FHEDT_USE_CACHE=0 ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" --require-gpu-palette \
    fhedt/gpu_apply 0 0 1 || status=$?
test "$status" -ne 77 || {
    echo "ok 1 # SKIP GPU palette backend unavailable"
    exit 0
}
test "$status" -eq 0 || {
    echo "not ok 1 - gpu palette p1"
    exit 0
}
echo "ok 1 - gpu palette p1"
exit 0
