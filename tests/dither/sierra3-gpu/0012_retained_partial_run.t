#!/bin/sh
# Verify sierra3 device retained partial run.
set -eux
echo "1..1"
set -v

status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "dither/sierra3-gpu/0012_retained_partial_run" || status=$?
test "$status" -ne 77 || {
    echo "ok 1 # SKIP required GPU capability is unavailable"
    exit 0
}
test "$status" -eq 0 || {
    echo "not ok 1 - sierra3 device retained partial run"
    exit 0
}
echo "ok 1 - sierra3 device retained partial run"
exit 0
