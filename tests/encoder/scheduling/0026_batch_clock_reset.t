#!/bin/sh
# Policy: docs/threading/encoder.md
set -eux
echo '1..1'
set -v
status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "encoder/scheduling/0026_batch_clock_reset" || status=$?
test "$status" -ne 77 || {
    echo 'ok 1 # SKIP thread support unavailable'
    exit 0
}
test "$status" -eq 0 || {
    echo 'not ok 1 - A missing worker clock resets the next frame'
    exit 0
}
echo 'ok 1 - A missing worker clock resets the next frame'
exit 0
