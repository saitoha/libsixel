#!/bin/sh
# Policy: docs/encoder/palette-application/lookup-policy/5bit.md
# Verify fractional caches survive interleaving with another policy.
set -eux
echo "1..1"
set -v
status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" gpu/5bit_fractional_policy_isolation || status=$?
test "$status" -ne 77 || {
    echo "ok 1 # SKIP GPU fractional probe unavailable"
    exit 0
}
test "$status" -eq 0 || {
    echo "not ok 1 - 5bit fractional policy isolation"
    exit 0
}
echo "ok 1 - 5bit fractional policy isolation"
exit 0
