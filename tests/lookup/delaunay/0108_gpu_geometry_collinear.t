#!/bin/sh
# Policy: docs/encoder/palette-application/lookup-policy/delaunay.md
# gpu geometry collinear.
set -eux
echo "1..1"
set -v

status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" --require-gpu-palette \
    delaunay/gpu_geometry 3 || status=$?
test "$status" -ne 77 || {
    echo "ok 1 # SKIP GPU palette backend unavailable"
    exit 0
}
test "$status" -eq 0 || {
    echo "not ok 1 - gpu geometry collinear"
    exit 0
}
echo "ok 1 - gpu geometry collinear"
exit 0
