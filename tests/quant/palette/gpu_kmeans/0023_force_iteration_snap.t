#!/bin/sh
# Policy: docs/encoder/palette-construction/gpu-kmeans.md

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    echo "1..0 # SKIP img2sixel is disabled"
    exit 0
}

echo "1..1"
set -v

conversion_status=0
trace=$(set +xv; SIXEL_TRACE_TOPIC=kmeans_backend \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --precision=8bit -Gforce -Qkmeans:backend=gpu:seed=1 --snap-policy=nearest:timing=iteration \
    -Fnone -p 16 -o /dev/null "${TOP_SRCDIR}/tests/data/inputs/snake_16.png" 2>&1) || conversion_status=$?
test "$conversion_status" -ne 0 || {
    echo "not ok 1 - forced GPU conversion unexpectedly succeeded"
    exit 0
}
case "$trace" in
    *gpu\ k-means:\ feedback\ or\ iteration\ snap\ is\ not\ supported.*) true ;;
    *) false ;;
esac || {
    echo "not ok 1 - force iteration snap backend selection"
    printf '# %s\n' "$trace"
    exit 0
}
echo "ok 1 - force iteration snap"
exit 0
