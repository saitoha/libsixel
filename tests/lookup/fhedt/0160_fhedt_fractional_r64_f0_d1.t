#!/bin/sh
# Policy: docs/encoder/palette-application/lookup-policy/fhedt.md
set -eux
echo "1..1"
set -v
status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" gpu/fhedt_fractional_r64_f0_d1 || status=$?
test "$status" -ne 77 || {
    echo "ok 1 # SKIP Metal device unavailable"
    exit 0
}
test "$status" -eq 0 || {
    echo "not ok 1 - fhedt fractional r64 f0 d1"
    exit 0
}
echo "ok 1 - fhedt fractional r64 f0 d1"
exit 0
