#!/bin/sh
# GPU sampling needs sustained headroom and respects the CPU policy.
set -eux
echo '1..1'
set -v
status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "palette/sampling/0107_sample_vulkan_headroom" || status=$?
test "$status" -ne 77 || {
    echo 'ok 1 # SKIP Vulkan is unavailable'
    exit 0
}
test "$status" -eq 0 || {
    echo 'not ok 1 - Vulkan sampling headroom selection'
    exit 0
}
echo 'ok 1 - Vulkan sampling headroom selection'
exit 0
