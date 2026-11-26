#!/bin/sh
set -eux
test "${HAVE_IMG2SIXEL-}" = 1 || {
    echo '1..0 # SKIP requires img2sixel'
    exit 0
}
probe_status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" gpu-kmeans/resident-available || probe_status=$?
test "$probe_status" -ne 77 || {
    echo '1..0 # SKIP requires a native GPU'
    exit 0
}
echo '1..1'
set -v
test "$probe_status" -eq 0 || {
    printf '\n%s\n' 'not ok 1 - GPU availability probe failed'
    exit 0
}
trace=$(set +xv; SIXEL_TRACE_TOPIC=palette_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -.8bit -Gforce -Fward -p32 \
    --lookup-policy=6bit -dnone -Qheckbert -shistogram -o /dev/null \
    "${TOP_SRCDIR}/tests/data/inputs/snake_64.png" 2>&1) || {
    printf '\n%s\n' 'not ok 1 - Heckbert Ward encoding failed'
    printf '# %s\n' "$trace"
    exit 0
}
case "$trace" in
    *LSXWARD1*backend=metal*target=32*merges=*) true ;;
    *LSXWARD1*backend=vulkan*target=32*merges=*) true ;;
    *) false ;;
esac || {
    printf '\n%s\n' 'not ok 1 - Heckbert did not use GPU Ward'
    printf '# %s\n' "$trace"
    exit 0
}
printf '\n%s\n' 'ok 1 - Heckbert encodes with GPU Ward'
exit 0
