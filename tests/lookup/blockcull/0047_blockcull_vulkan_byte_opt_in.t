#!/bin/sh
# Vulkan byte batch lookup requires explicit consumer opt-in.
set -eux
echo "1..1"
set -v
status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" gpu/blockcull_vulkan_byte_opt_in || status=$?
test "$status" -ne 77 || {
    echo "ok 1 # SKIP Vulkan device unavailable"
    exit 0
}
test "$status" -eq 0 || {
    echo "not ok 1 - blockcull byte batch opt-in"
    exit 0
}
echo "ok 1 - blockcull byte batch opt-in"
exit 0
