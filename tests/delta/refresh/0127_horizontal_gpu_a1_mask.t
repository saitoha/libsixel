#!/bin/sh
# Verify horizontal gpu a1 mask.
# Policy: docs/encoder/output/delta-encoding.md
set -eux
echo "1..1"
set -v
status=0
trace=$(SIXEL_TRACE_TOPIC=gpu_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" --require-gpu-palette \
    delta/refresh/0127_horizontal_gpu_a1_mask 2>&1) || status=$?
test "$status" -ne 77 || {
    echo "ok 1 # SKIP GPU palette backend unavailable"
    exit 0
}
test "$status" -eq 0 || {
    echo "not ok 1 - horizontal gpu a1 mask output"
    exit 0
}
case "$trace" in
    *"LSXGPU2|consumer=palette-apply|kernel="*) true ;;
    *) false ;;
esac || {
    echo "not ok 1 - horizontal gpu a1 mask dispatch"
    exit 0
}
echo "ok 1 - horizontal gpu a1 mask"
exit 0
