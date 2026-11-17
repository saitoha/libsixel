#!/bin/sh
# Verify lso2 device retained partial resolve.
set -eux
echo "1..1"
set -v

status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "dither/lso2-gpu/0085_retained_partial_resolve" || status=$?
test "$status" -ne 77 || {
    echo "ok 1 # SKIP required GPU capability is unavailable"
    exit 0
}
test "$status" -eq 0 || {
    echo "not ok 1 - lso2 device retained partial resolve"
    exit 0
}
echo "ok 1 - lso2 device retained partial resolve"
exit 0
