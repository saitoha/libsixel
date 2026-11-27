#!/bin/sh
# Policy: docs/encoder/palette-construction/quantization/README.md
# A histogram the Vulkan Heckbert solver declines stays on the CPU under
# the force policy and yields the CPU-only palette.
set -eux
echo '1..1'
set -v
status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "palette/heckbert/0160_gpu_force_declined" || status=$?
test "$status" -ne 77 || {
    printf '\n%s\n' 'ok 1 # SKIP requires a Vulkan Heckbert solver'
    exit 0
}
test "$status" -eq 0 || {
    printf '\n%s\n' 'not ok 1 - forced declined histogram palette'
    exit 0
}
printf '\n%s\n' 'ok 1 - forced declined histogram palette'
exit 0
