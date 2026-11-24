#!/bin/sh
# Run the GPU palette threshold strtoul() contract test.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "gpu-palette/0001_gpu_palette_threshold_strtoul" || {
    printf '\n%s\n' "not ok 1 - GPU palette threshold preserves strtoul semantics"
    exit 0
}

printf '\n%s\n' "ok 1 - GPU palette threshold preserves strtoul semantics"
exit 0
