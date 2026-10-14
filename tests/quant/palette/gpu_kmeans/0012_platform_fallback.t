#!/bin/sh
# Policy: docs/encoder/palette-construction/gpu-kmeans.md

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    echo "1..0 # SKIP img2sixel is disabled"
    exit 0
}

probe_status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" gpu-kmeans/available || probe_status=$?
test "$probe_status" -ne 0 || {
    echo "1..0 # SKIP Metal device is available"
    exit 0
}
echo "1..1"
set -v

test "$probe_status" -eq 77 || {
    echo "not ok 1 - Metal device probe failed"
    exit 0
}

trace=$(set +xv; SIXEL_TRACE_TOPIC=kmeans_backend \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -Gauto -Qkmeans:backend=gpu:seed=1 \
    -Fnone -p 16 -o /dev/null "${TOP_SRCDIR}/tests/data/inputs/snake_16.png" 2>&1) || {
    echo "not ok 1 - platform fallback conversion"
    exit 0
}
case "$trace" in
    *LSXKMEANS1\|requested=gpu\|effective=cpu\|*) true ;;
    *) false ;;
esac || {
    echo "not ok 1 - platform fallback backend selection"
    printf '# %s\n' "$trace"
    exit 0
}
echo "ok 1 - platform fallback"
exit 0
