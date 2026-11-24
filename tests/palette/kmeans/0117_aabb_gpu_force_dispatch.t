#!/bin/sh
# TODO: GPU FORCE should execute AABB pruning when supported.
set -eux
test "${HAVE_IMG2SIXEL-}" = 1 || {
    echo "1..0 # SKIP img2sixel is disabled"
    exit 0
}
probe_status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" gpu-kmeans/available || probe_status=$?
test "$probe_status" -ne 77 || {
    echo '1..0 # SKIP GPU device is unavailable'
    exit 0
}
echo '1..1'
set -v
test "$probe_status" -eq 0 || {
    printf '\n%s\n' 'not ok 1 - GPU availability probe failed'
    exit 0
}
trace=$(set +xv; SIXEL_TRACE_TOPIC=kmeans_backend \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -Gforce -Fnone -p 16 \
    -Qkmeans:backend=gpu:prune=aabb:seed=1 -o /dev/null \
    "${TOP_SRCDIR}/tests/data/inputs/snake_16.png" 2>&1) || {
    printf '\n%s\n' 'not ok 1 - GPU FORCE rejected AABB pruning'
    printf '# %s\n' "$trace"
    exit 0
}
case "$trace" in
    *effective=metal*prune=aabb*gpu_used=1*|*effective=vulkan*prune=aabb*gpu_used=1*) true ;;
    *) false ;;
esac || {
    printf '\n%s\n' 'not ok 1 - GPU FORCE did not execute AABB pruning'
    printf '# %s\n' "$trace"
    exit 0
}
printf '\n%s\n' 'ok 1 - GPU FORCE executed AABB pruning'
exit 0
