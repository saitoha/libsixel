#!/bin/sh
# Verify auto policy off.
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
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -Goff -Qkmeans:backend=auto:seed=1 \
    -Fnone -p 16 -o /dev/null "${TOP_SRCDIR}/tests/data/inputs/snake_16.png" 2>&1) || conversion_status=$?
test "$conversion_status" -eq 0 || {
    echo "not ok 1 - auto policy off status"
    printf '# %s\n' "$trace"
    exit 0
}
case "$trace" in
    *requested=auto*effective=cpu*reason=policy-off*) true ;;
    *) false ;;
esac || {
    echo "not ok 1 - auto policy off contract"
    printf '# %s\n' "$trace"
    exit 0
}
echo "ok 1 - auto policy off"
exit 0
