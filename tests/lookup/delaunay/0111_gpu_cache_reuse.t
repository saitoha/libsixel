#!/bin/sh
# Policy: docs/encoder/palette-application/lookup-policy/delaunay.md
# gpu cache reuse.
set -eux
echo "1..1"
set -v

status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" --require-gpu-lookup delaunay \
    delaunay/gpu_cache 0 || status=$?
test "$status" -ne 77 || {
    printf '\n%s\n' "ok 1 # SKIP GPU delaunay lookup unavailable"
    exit 0
}
test "$status" -eq 0 || {
    printf '\n%s\n' "not ok 1 - gpu cache reuse"
    exit 0
}
printf '\n%s\n' "ok 1 - gpu cache reuse"
exit 0
