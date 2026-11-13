#!/bin/sh
# Policy: docs/encoder/palette-application/lookup-policy/fhedt.md
set -eux
echo "1..1"
set -v
status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" gpu/fhedt_fractional_r256_f0_d1 || status=$?
test "$status" -ne 77 || {
    echo "ok 1 # SKIP GPU fractional probe unavailable"
    exit 0
}
test "$status" -eq 0 || {
    echo "not ok 1 - fhedt fractional r256 f0 d1"
    exit 0
}
echo "ok 1 - fhedt fractional r256 f0 d1"
exit 0
