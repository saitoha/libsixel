#!/bin/sh
# Policy: docs/encoder/palette-construction/quantization/README.md
# The GPU Heckbert solver partitions the histogram and declines inputs
# outside its domain without writing any output.
set -eux
echo '1..1'
set -v
status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "palette/heckbert/0158_gpu_solver_partition" || status=$?
test "$status" -ne 77 || {
    printf '\n%s\n' 'ok 1 # SKIP requires a GPU Heckbert solver'
    exit 0
}
test "$status" -eq 0 || {
    printf '\n%s\n' 'not ok 1 - GPU Heckbert solver partition'
    exit 0
}
printf '\n%s\n' 'ok 1 - GPU Heckbert solver partition'
exit 0
