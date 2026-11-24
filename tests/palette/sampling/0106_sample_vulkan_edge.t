#!/bin/sh
# Exact Vulkan histogram against scalar clipped-edge sampling.
set -eux
echo '1..1'
set -v
status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "palette/sampling/0106_sample_vulkan_edge" || status=$?
test "$status" -ne 77 || {
    printf '\n%s\n' 'ok 1 # SKIP Vulkan is unavailable'
    exit 0
}
test "$status" -eq 0 || {
    printf '\n%s\n' 'not ok 1 - edge Vulkan sampling exactness'
    exit 0
}
printf '\n%s\n' 'ok 1 - edge Vulkan sampling exactness'
exit 0
