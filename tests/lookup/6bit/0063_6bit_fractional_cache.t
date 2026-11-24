#!/bin/sh
# Policy: docs/encoder/palette-application/lookup-policy/6bit.md
set -eux
echo "1..1"
set -v
status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" gpu/6bit_fractional_cache || status=$?
test "$status" -ne 77 || {
    printf '\n%s\n' "ok 1 # SKIP Metal device unavailable"
    exit 0
}
test "$status" -eq 0 || {
    printf '\n%s\n' "not ok 1 - 6bit fractional cache"
    exit 0
}
printf '\n%s\n' "ok 1 - 6bit fractional cache"
exit 0
