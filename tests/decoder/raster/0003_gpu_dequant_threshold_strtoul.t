#!/bin/sh
# Run the GPU dequant threshold strtoul() contract test.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "gpu-dequant/0001_gpu_dequant_threshold_strtoul" || {
    printf '\n%s\n' "not ok 1 - GPU dequant threshold preserves strtoul semantics"
    exit 0
}

printf '\n%s\n' "ok 1 - GPU dequant threshold preserves strtoul semantics"
exit 0
