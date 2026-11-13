#!/bin/sh
# Policy: docs/encoder/palette-application/lookup-policy/5bit.md
# Verify completed fractional tables are reused by the next solve.
set -eux
echo "1..1"
set -v
status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" gpu/5bit_fractional_reuse || status=$?
test "$status" -ne 77 || {
    echo "ok 1 # SKIP GPU fractional probe unavailable"
    exit 0
}
test "$status" -eq 0 || {
    echo "not ok 1 - 5bit fractional table reuse"
    exit 0
}
echo "ok 1 - 5bit fractional table reuse"
exit 0
