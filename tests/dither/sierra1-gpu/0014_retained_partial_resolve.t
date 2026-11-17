#!/bin/sh
# Verify sierra1 device retained partial resolve.
set -eux
echo "1..1"
set -v

status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "dither/sierra1-gpu/0014_retained_partial_resolve" || status=$?
test "$status" -ne 77 || {
    echo "ok 1 # SKIP required GPU capability is unavailable"
    exit 0
}
test "$status" -eq 0 || {
    echo "not ok 1 - sierra1 device retained partial resolve"
    exit 0
}
echo "ok 1 - sierra1 device retained partial resolve"
exit 0
