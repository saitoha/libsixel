#!/bin/sh
# Exact Vulkan histogram against scalar random-frame sampling.
set -eux
echo '1..1'
set -v
status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "palette/sampling/0105_sample_vulkan_random" || status=$?
test "$status" -ne 77 || {
    echo 'ok 1 # SKIP Vulkan is unavailable'
    exit 0
}
test "$status" -eq 0 || {
    echo 'not ok 1 - random Vulkan sampling exactness'
    exit 0
}
echo 'ok 1 - random Vulkan sampling exactness'
exit 0
