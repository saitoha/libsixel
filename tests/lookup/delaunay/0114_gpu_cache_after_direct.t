#!/bin/sh
# Policy: docs/encoder/palette-application/lookup-policy/delaunay.md
# gpu cache after direct.
set -eux
echo "1..1"
set -v

status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" --require-gpu-lookup delaunay \
    delaunay/gpu_cache 3 || status=$?
test "$status" -ne 77 || {
    echo "ok 1 # SKIP GPU delaunay lookup unavailable"
    exit 0
}
test "$status" -eq 0 || {
    echo "not ok 1 - gpu cache after direct"
    exit 0
}
echo "ok 1 - gpu cache after direct"
exit 0
