#!/bin/sh
# Policy: docs/encoder/palette-application/lookup-policy/delaunay.md
# gpu bluenoise keep p2.
set -eux
echo "1..1"
set -v

status=0
trace=$(set +xv; SIXEL_TRACE_TOPIC=gpu_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" --require-gpu-palette \
    delaunay/gpu_exact 1 1 2 2>&1) || status=$?
test "$status" -ne 77 || {
    echo "ok 1 # SKIP GPU palette backend unavailable"
    exit 0
}
test "$status" -eq 0 || {
    printf "# %s\n" "$trace"
    echo "not ok 1 - gpu bluenoise keep p2"
    exit 0
}
case "$trace" in
    *'kernel=delaunay'*) true ;;
    *) false ;;
esac || {
    printf "# %s\n" "$trace"
    echo "not ok 1 - gpu bluenoise keep p2 execution path"
    exit 0
}
echo "ok 1 - gpu bluenoise keep p2"
exit 0
