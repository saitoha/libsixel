#!/bin/sh
# ordered none keep p4 exact.
# Policy: docs/encoder/palette-application/lookup-policy/ordered.md
set -eux

echo "1..1"
set -v

status=0
trace=$(set +xv; SIXEL_TRACE_TOPIC=gpu_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" --require-gpu-palette \
    gpu-palette/0023_ordered_exact 0 1 4 2>&1) || status=$?
test "$status" -ne 77 || {
    echo "ok 1 # SKIP GPU palette backend unavailable"
    exit 0
}
test "$status" -eq 0 || {
    printf "# %s\n" "$trace"
    echo "not ok 1 - ordered none keep p4 exact"
    exit 0
}
case "$trace" in
    *'kernel=ordered'*) true ;;
    *) false ;;
esac || {
    printf "# %s\n" "$trace"
    echo "not ok 1 - ordered none keep p4 exact execution path"
    exit 0
}
echo "ok 1 - ordered none keep p4 exact"
exit 0
