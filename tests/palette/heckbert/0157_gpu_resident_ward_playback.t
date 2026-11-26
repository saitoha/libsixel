#!/bin/sh
set -eux
probe_status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" gpu-kmeans/resident-available ward || probe_status=$?
test "$probe_status" -ne 77 || {
    echo '1..0 # SKIP requires native GPU Ward and SIXEL support'
    exit 0
}
echo '1..1'
set -v
test "$probe_status" -eq 0 || {
    printf '\n%s\n' 'not ok 1 - GPU availability probe failed'
    exit 0
}
trace=$(set +xv; SIXEL_TRACE_TOPIC=palette_contract \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" gpu-heckbert/resident-playback ward 2>&1) || {
    printf '\n%s\n' 'not ok 1 - Ward playback must match with one submission per frame'
    printf '# %s\n' "$trace"
    exit 0
}
case "$trace" in
    *LSXBODYRESIDENT1*LSXWARDRESIDENT1*lloyd=3*\
LSXBODYRESIDENT1*LSXWARDRESIDENT1*lloyd=3*) true ;;
    *) false ;;
esac || {
    printf '\n%s\n' 'not ok 1 - direct and playback calls did not both use resident Ward and SIXEL spelling'
    printf '# %s\n' "$trace"
    exit 0
}
printf '\n%s\n' 'ok 1 - direct and playback outputs match after resident Ward and SIXEL spelling'
exit 0
