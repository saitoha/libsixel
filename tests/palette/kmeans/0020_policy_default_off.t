#!/bin/sh
# Policy: docs/encoder/palette-construction/gpu-kmeans.md

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    echo "1..0 # SKIP img2sixel is disabled"
    exit 0
}

echo "1..1"
set -v

trace=$(set +xv; SIXEL_TRACE_TOPIC=kmeans_backend SIXEL_GPU_POLICY='' \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -Qkmeans:backend=gpu:seed=1 \
    -Fnone -p 16 -o /dev/null "${TOP_SRCDIR}/tests/data/inputs/snake_16.png" 2>&1) || {
    echo "not ok 1 - policy default off conversion"
    exit 0
}
case "$trace" in
    *LSXKMEANS1\|requested=gpu\|effective=cpu\|*) true ;;
    *) false ;;
esac || {
    echo "not ok 1 - policy default off backend selection"
    printf '# %s\n' "$trace"
    exit 0
}
echo "ok 1 - policy default off"
exit 0
