#!/bin/sh
set -eux
test "${HAVE_IMG2SIXEL-}" = 1 || {
    echo '1..0 # SKIP requires img2sixel'
    exit 0
}
probe_status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" gpu-kmeans/resident-available || probe_status=$?
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
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -.8bit -Gforce -Fnone -p64 \
    --lookup-policy=6bit -dnone -4adaptive-grid:solid=off -5hard \
    -Qkmeans:backend=gpu:inittype=pca:binbits=5:iter=6 -_reversible \
    -aedges:cover_mode=hard:cover_grow=1 -o /dev/null \
    "${TOP_SRCDIR}/tests/data/inputs/snake_64.png" 2>&1) || {
    printf '\n%s\n' 'not ok 1 - GPU palette and indexed encoding failed'
    printf '# %s\n' "$trace"
    exit 0
}
case "$trace" in
    *LSXKMRESIDENT1*backend=metal*colors=*snap=1*cover=3*soft=0*) true ;;
    *) false ;;
esac || {
    printf '\n%s\n' 'not ok 1 - snap and hard cover did not use resident Metal'
    printf '# %s\n' "$trace"
    exit 0
}
printf '\n%s\n' 'ok 1 - Metal snap and hard cover complete before indexed publication'
exit 0
