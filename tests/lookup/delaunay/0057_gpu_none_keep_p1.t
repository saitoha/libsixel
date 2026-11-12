#!/bin/sh
# Policy: docs/encoder/palette-application/lookup-policy/delaunay.md
# gpu none keep p1.
set -eux
echo "1..1"
set -v

status=0
trace=$(set +xv; SIXEL_TRACE_TOPIC=gpu_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" --require-gpu-lookup delaunay \
    delaunay/gpu_exact 0 1 1 2>&1) || status=$?
test "$status" -ne 77 || {
    echo "ok 1 # SKIP GPU delaunay lookup unavailable"
    exit 0
}
test "$status" -eq 0 || {
    printf "# %s\n" "$trace"
    echo "not ok 1 - gpu none keep p1"
    exit 0
}
case "$trace" in
    *'kernel=delaunay'*) true ;;
    *) false ;;
esac || {
    printf "# %s\n" "$trace"
    echo "not ok 1 - gpu none keep p1 execution path"
    exit 0
}
echo "ok 1 - gpu none keep p1"
exit 0
