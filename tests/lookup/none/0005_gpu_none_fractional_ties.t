#!/bin/sh
# Policy: docs/encoder/palette-application/lookup-policy/none.md
set -eux
echo "1..1"
set -v
status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" gpu/none_fractional_ties || status=$?
test "$status" -ne 77 || {
    echo "ok 1 # SKIP GPU fractional probe unavailable"
    exit 0
}
test "$status" -eq 0 || {
    echo "not ok 1 - none fractional ties"
    exit 0
}
echo "ok 1 - none fractional ties"
exit 0
