#!/bin/sh
# Verify auto minimum through the production selection trace.
# Policy: docs/encoder/palette-construction/gpu-kmeans.md
set -eux
echo "1..1"
set -v
run_status=0
trace=$(SIXEL_TRACE_TOPIC=kmeans_backend ${SIXEL_RUNTIME-} \
    "${TEST_RUNNER_PATH}" "gpu-kmeans/0090_auto_minimum" 2>&1) || run_status=$?
test "$run_status" -eq 0 || {
    printf '%s\n' "$trace"
    echo "not ok 1 - auto minimum status"
    exit 0
}
case "$trace" in
    *requested=auto*effective=cpu*reason=workload-small*M=12288*K=256*gpu_used=0*cpu_workspace=1*) true ;;
    *)
    printf '%s\n' "$trace"
    echo "not ok 1 - auto minimum dispatch"
    exit 0 ;;
esac
echo "ok 1 - auto minimum"
exit 0
