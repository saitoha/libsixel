#!/bin/sh
# ordered table reuse preserves exact indexes.
# Policy: docs/encoder/palette-application/lookup-policy/ordered.md
set -eux

echo "1..1"
set -v

status=0
trace=$(set +xv; SIXEL_TRACE_TOPIC=gpu_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" --require-gpu-palette \
    gpu-palette/0023_ordered_exact 3 0 33 2>&1) || status=$?
test "$status" -ne 77 || {
    echo "ok 1 # SKIP GPU palette backend unavailable"
    exit 0
}
test "$status" -eq 0 || {
    printf "# %s\n" "$trace"
    echo "not ok 1 - ordered table reuse preserves exact indexes"
    exit 0
}
case "$trace" in
    *'lookup=ordered|table=rebuild'*'lookup=ordered|table=reuse'*) true ;;
    *) false ;;
esac || {
    printf "# %s\n" "$trace"
    echo "not ok 1 - ordered table reuse preserves exact indexes execution path"
    exit 0
}
echo "ok 1 - ordered table reuse preserves exact indexes"
exit 0
