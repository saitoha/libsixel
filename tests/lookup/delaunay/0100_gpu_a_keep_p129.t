#!/bin/sh
# Policy: docs/encoder/palette-application/lookup-policy/delaunay.md
# gpu a keep p129.
set -eux
echo "1..1"
set -v

status=0
trace=$(set +xv; SIXEL_TRACE_TOPIC=gpu_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" --require-gpu-lookup delaunay \
    delaunay/gpu_exact 4 1 129 2>&1) || status=$?
test "$status" -ne 77 || {
    echo "ok 1 # SKIP GPU delaunay lookup unavailable"
    exit 0
}
test "$status" -eq 0 || {
    printf "# %s\n" "$trace"
    echo "not ok 1 - gpu a keep p129"
    exit 0
}
case "$trace" in
    *'kernel=delaunay'*) true ;;
    *) false ;;
esac || {
    printf "# %s\n" "$trace"
    echo "not ok 1 - gpu a keep p129 execution path"
    exit 0
}
echo "ok 1 - gpu a keep p129"
exit 0
