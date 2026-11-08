#!/bin/sh
# Verify auto warm through the production selection trace.
# Policy: docs/encoder/palette-construction/gpu-kmeans.md
set -eux
echo "1..1"
set -v
run_status=0
trace=$(SIXEL_TRACE_TOPIC=kmeans_backend ${SIXEL_RUNTIME-} \
    "${TEST_RUNNER_PATH}" "gpu-kmeans/0089_auto_warm" 2>&1) || run_status=$?
test "$run_status" -ne 77 || {
    echo "ok 1 # SKIP Metal device is unavailable"
    exit 0
}
test "$run_status" -eq 0 || {
    printf '%s\n' "$trace"
    echo "not ok 1 - auto warm status"
    exit 0
}
case "$trace" in
    *requested=auto*effective=metal*reason=eligible*M=12288*K=256*gpu_used=1*cpu_workspace=0*) true ;;
    *)
    printf '%s\n' "$trace"
    echo "not ok 1 - auto warm dispatch"
    exit 0 ;;
esac
echo "ok 1 - auto warm"
exit 0
