#!/bin/sh
# Policy: docs/threading/encoder.md
# Verify gpu ahead t48 bytes.
set -eux
echo '1..1'
set -v
status=0
trace=$(set +xv; SIXEL_TRACE_TOPIC=gpu_contract ${SIXEL_RUNTIME-} \
    "${TEST_RUNNER_PATH}" --require-gpu-palette \
    encoder/planning/0187_gpu_ahead_t48_bytes 2>&1) || status=$?
test "$status" -ne 77 || {
    echo 'ok 1 # SKIP required thread or GPU support unavailable'
    exit 0
}
test "$status" -eq 0 || {
    printf '# %s\n' "$trace"
    echo 'not ok 1 - gpu ahead t48 bytes'
    exit 0
}
case "$trace" in
    *'LSXGPU2|consumer=palette-apply|kernel='*) true ;;
    *) false ;;
esac || {
    printf '# %s\n' "$trace"
    echo 'not ok 1 - GPU kernel was not dispatched'
    exit 0
}
echo 'ok 1 - gpu ahead t48 bytes'
exit 0
