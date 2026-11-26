#!/bin/sh
set -eux
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
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" gpu-heckbert/resident-playback 2>&1) || {
    printf '\n%s\n' 'not ok 1 - direct and playback GPU indexed outputs disagree'
    printf '# %s\n' "$trace"
    exit 0
}
case "$trace" in
    *LSXHBRESIDENT1*snap=1*soft=1*sample_backend=metal*histogram_backend=metal*\
LSXHBRESIDENT1*snap=1*soft=1*sample_backend=metal*histogram_backend=metal*) true ;;
    *LSXHBRESIDENT1*snap=1*soft=1*sample_backend=vulkan*histogram_backend=vulkan*\
LSXHBRESIDENT1*snap=1*soft=1*sample_backend=vulkan*histogram_backend=vulkan*) true ;;
    *) false ;;
esac || {
    printf '\n%s\n' 'not ok 1 - direct and playback calls did not both use resident GPU'
    printf '# %s\n' "$trace"
    exit 0
}
printf '\n%s\n' 'ok 1 - playback reuses the caller frame and emits the same indexed result'
exit 0
