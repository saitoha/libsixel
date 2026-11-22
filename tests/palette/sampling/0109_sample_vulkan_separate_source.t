#!/bin/sh
# A separate palette source takes the Vulkan scan and matches the CPU sample.
set -eux
echo '1..1'
set -v
status=0
_SIXEL_TEST_GPU_HEADROOM=gpu ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "palette/sampling/0109_sample_vulkan_separate_source" || status=$?
test "$status" -ne 77 || {
    echo 'ok 1 # SKIP Vulkan is unavailable'
    exit 0
}
test "$status" -eq 0 || {
    echo 'not ok 1 - separate-source Vulkan sampling exactness'
    exit 0
}
echo 'ok 1 - separate-source Vulkan sampling exactness'
exit 0
