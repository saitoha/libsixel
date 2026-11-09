#!/bin/sh
# Verify auto force feedback.
# Policy: docs/encoder/palette-construction/gpu-kmeans.md
set -eux
test "${HAVE_IMG2SIXEL-}" = 1 || {
    echo "1..0 # SKIP img2sixel is disabled"
    exit 0
}
echo "1..1"
set -v
conversion_status=0
trace=$(set +xv; SIXEL_TRACE_TOPIC=kmeans_backend,dither_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -Gforce -Qkmeans:backend=auto:feedback=1:seed=1 \
    -Fnone -p 16 -o /dev/null "${TOP_SRCDIR}/tests/data/inputs/snake_16.png" 2>&1) || conversion_status=$?
test "$conversion_status" -ne 0 || {
    echo "not ok 1 - auto force feedback status"
    printf '# %s\n' "$trace"
    exit 0
}
case "$trace" in
    *gpu\ k-means:\ feedback\ or\ iteration\ snap\ is\ not\ supported.*) true ;;
    *) false ;;
esac || {
    echo "not ok 1 - auto force feedback contract"
    printf '# %s\n' "$trace"
    exit 0
}
echo "ok 1 - auto force feedback"
exit 0
