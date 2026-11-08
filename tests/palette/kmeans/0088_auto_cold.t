#!/bin/sh
# Verify auto cold through the production selection trace.
# Policy: docs/encoder/palette-construction/gpu-kmeans.md
set -eux
echo "1..1"
set -v
run_status=0
trace=$(SIXEL_TRACE_TOPIC=kmeans_backend ${SIXEL_RUNTIME-} \
    "${TEST_RUNNER_PATH}" "gpu-kmeans/0088_auto_cold" 2>&1) || run_status=$?
test "$run_status" -eq 0 || {
    printf '%s\n' "$trace"
    echo "not ok 1 - auto cold status"
    exit 0
}
case "$trace" in
    *requested=auto*effective=cpu*reason=pipeline-cold*M=12288*K=256*gpu_used=0*cpu_workspace=1*) true ;;
    *)
    printf '%s\n' "$trace"
    echo "not ok 1 - auto cold dispatch"
    exit 0 ;;
esac
echo "ok 1 - auto cold"
exit 0
