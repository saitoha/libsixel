#!/bin/sh
# Policy: docs/encoder/palette-application/lookup-policy/fhedt.md
set -eux
echo "1..1"
set -v
status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" gpu/fhedt_fractional_cache || status=$?
test "$status" -ne 77 || {
    printf '\n%s\n' "ok 1 # SKIP Metal device unavailable"
    exit 0
}
test "$status" -eq 0 || {
    printf '\n%s\n' "not ok 1 - fhedt fractional cache"
    exit 0
}
printf '\n%s\n' "ok 1 - fhedt fractional cache"
exit 0
