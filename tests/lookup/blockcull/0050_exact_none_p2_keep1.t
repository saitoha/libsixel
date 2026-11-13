#!/bin/sh
# Verify exact none p2 keep1.
# Policy: docs/encoder/palette-application/lookup-policy/blockcull.md
set -eux
echo "1..1"
set -v

status=0
trace=$(set +xv; SIXEL_TRACE_TOPIC=gpu_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" --require-gpu-lookup blockcull \
    "lookup/blockcull/0050_exact_none_p2_keep1" 2>&1) || status=$?
test "$status" -ne 77 || {
    echo "ok 1 # SKIP required GPU capability is unavailable"
    exit 0
}
test "$status" -eq 0 || {
    printf '# %s\n' "$trace"
    echo "not ok 1 - exact none p2 keep1"
    exit 0
}
# Metal uses GEMM for short palettes, or a scalar fallback without matrices.
# Vulkan uses the blockcull program at every palette size.
case "$trace" in
    *'kernel=blockcull'*|*'kernel=gemm'*|*'kernel=direct'*) true ;;
    *) false ;;
esac || {
    printf '# %s\n' "$trace"
    echo "not ok 1 - expected GPU lookup kernel was not selected"
    exit 0
}
echo "ok 1 - exact none p2 keep1"
exit 0
