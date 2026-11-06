#!/bin/sh
# Policy: docs/encoder/palette-application/lookup-policy/gridlut.md
set -eux
echo "1..1"
set -v
status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" gpu/gridlut_fractional_cache || status=$?
test "$status" -ne 77 || {
    echo "ok 1 # SKIP Metal device unavailable"
    exit 0
}
test "$status" -eq 0 || {
    echo "not ok 1 - gridlut fractional cache"
    exit 0
}
echo "ok 1 - gridlut fractional cache"
exit 0
