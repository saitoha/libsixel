#!/bin/sh
# Verify stucki device retained full resolve.
set -eux
echo "1..1"
set -v

status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "dither/stucki-gpu/0013_retained_full_resolve" || status=$?
test "$status" -ne 77 || {
    echo "ok 1 # SKIP required GPU capability is unavailable"
    exit 0
}
test "$status" -eq 0 || {
    echo "not ok 1 - stucki device retained full resolve"
    exit 0
}
echo "ok 1 - stucki device retained full resolve"
exit 0
