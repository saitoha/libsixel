#!/bin/sh
# Policy: docs/encoder/palette-application/lookup-policy/blockcull.md
set -eux
echo "1..1"
set -v
status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" gpu/blockcull_batch_deferred || status=$?
test "$status" -ne 77 || {
    echo "ok 1 # SKIP Metal device unavailable"
    exit 0
}
test "$status" -eq 0 || {
    echo "not ok 1 - blockcull batch deferred"
    exit 0
}
echo "ok 1 - blockcull batch deferred"
exit 0
