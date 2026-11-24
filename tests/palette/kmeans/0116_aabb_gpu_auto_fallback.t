#!/bin/sh
# GPU AUTO permits the explicitly requested CPU-only pruning policy.
set -eux
test "${HAVE_IMG2SIXEL-}" = 1 || {
    echo "1..0 # SKIP img2sixel is disabled"
    exit 0
}
echo '1..1'
set -v
trace=$(set +xv; SIXEL_TRACE_TOPIC=kmeans_backend \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -Gauto -Fnone -p 16 \
    -Qkmeans:backend=gpu:prune=aabb:seed=1 -o /dev/null \
    "${TOP_SRCDIR}/tests/data/inputs/snake_16.png" 2>&1) || {
    printf '\n%s\n' 'not ok 1 - GPU AUTO rejected CPU AABB fallback'
    printf '# %s\n' "$trace"
    exit 0
}
case "$trace" in
    *effective=cpu*reason=explicit-prune*prune=aabb*cpu_workspace=1*) true ;;
    *) false ;;
esac || {
    printf '\n%s\n' 'not ok 1 - GPU AUTO did not use CPU AABB pruning'
    printf '# %s\n' "$trace"
    exit 0
}
printf '\n%s\n' 'ok 1 - GPU AUTO uses CPU AABB pruning'
exit 0
