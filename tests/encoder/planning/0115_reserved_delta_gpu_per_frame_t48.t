#!/bin/sh
# Policy: docs/encoder/palette-construction/pipeline/README.md
# Reserved UI appears and disappears with gpu per frame t48
set -eux
echo "1..1"
set -v

status=0
trace=$(set +xv; SIXEL_TRACE_TOPIC=gpu_contract ${SIXEL_RUNTIME-} \
    "${TEST_RUNNER_PATH}" --require-gpu-palette \
    planner/pipeline/0115_reserved_delta_gpu_per_frame_t48 2>&1) || status=$?
test "$status" -ne 77 || {
    echo "ok 1 # SKIP GPU palette backend unavailable"
    exit 0
}
test "$status" -eq 0 || {
    printf '# %s\n' "$trace"
    echo "not ok 1 - Reserved UI appears and disappears with gpu per frame t48"
    exit 0
}
case "$trace" in
    *'LSXGPU2|consumer=palette-apply|kernel='*) true ;;
    *) false ;;
esac || {
    printf '# %s\n' "$trace"
    echo "not ok 1 - reserved UI did not dispatch a GPU kernel"
    exit 0
}
echo "ok 1 - Reserved UI appears and disappears with gpu per frame t48"
exit 0
