#!/bin/sh
set -eux
probe_status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" gpu-kmeans/resident-available || probe_status=$?
test "$probe_status" -ne 77 || {
    echo '1..0 # SKIP requires Metal'
    exit 0
}
echo '1..1'
set -v
test "$probe_status" -eq 0 || {
    echo 'not ok 1 - GPU availability probe failed'
    exit 0
}
trace=$(set +xv; SIXEL_TRACE_TOPIC=palette_contract \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" gpu-heckbert/resident-playback 2>&1) || {
    echo 'not ok 1 - direct and playback Metal indexed outputs disagree'
    printf '# %s\n' "$trace"
    exit 0
}
case "$trace" in
    *LSXHBRESIDENT1*snap=1*soft=1*LSXHBRESIDENT1*snap=1*soft=1*) true ;;
    *) false ;;
esac || {
    echo 'not ok 1 - direct and playback calls did not both use resident Metal'
    printf '# %s\n' "$trace"
    exit 0
}
echo 'ok 1 - playback reuses the caller frame and emits the same indexed result'
exit 0
