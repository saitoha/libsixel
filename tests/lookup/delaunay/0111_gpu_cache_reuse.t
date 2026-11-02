#!/bin/sh
# Policy: docs/encoder/palette-application/lookup-policy/delaunay.md
# gpu cache reuse.
set -eux
echo "1..1"
set -v

status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" --require-gpu-palette \
    delaunay/gpu_cache 0 || status=$?
test "$status" -ne 77 || {
    echo "ok 1 # SKIP GPU palette backend unavailable"
    exit 0
}
test "$status" -eq 0 || {
    echo "not ok 1 - gpu cache reuse"
    exit 0
}
echo "ok 1 - gpu cache reuse"
exit 0
