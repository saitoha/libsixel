#!/bin/sh
set -eux
test "${HAVE_IMG2SIXEL-}" = 1 || {
    echo '1..0 # SKIP requires img2sixel'
    exit 0
}
probe_status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" gpu-kmeans/resident-available || probe_status=$?
test "$probe_status" -ne 77 || {
    echo '1..0 # SKIP requires Metal'
    exit 0
}
echo '1..1'
set -v
test "$probe_status" -eq 0 || {
    echo 'not ok 1 - Metal availability probe failed'
    exit 0
}
trace=$(set +xv; SIXEL_TRACE_TOPIC=palette_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -.8bit -Gforce -Fnone -p64 \
    --lookup-policy=6bit -dnone -4adaptive-grid:solid=off -fnorm -shistogram \
    -Qheckbert:cut=sse:order=sse -_reversible:timing=once \
    -aedges:cover_mode=hard:cover_grow=1 -o /dev/null \
    "${TOP_SRCDIR}/tests/data/inputs/snake_64.png" 2>&1) || {
    echo 'not ok 1 - Heckbert indexed encoding failed'
    printf '# %s\n' "$trace"
    exit 0
}
case "$trace" in
    *LSXHBRESIDENT1*backend=metal*colors=*snap=1*cover=3*soft=0*\
sample_backend=metal*histogram_backend=metal*) true ;;
    *) false ;;
esac || {
    echo 'not ok 1 - Heckbert snap and hard cover did not use resident Metal'
    printf '# %s\n' "$trace"
    exit 0
}
echo 'ok 1 - Heckbert produces indexed output after Metal snap and hard cover'
exit 0
