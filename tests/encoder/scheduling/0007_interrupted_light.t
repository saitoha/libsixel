#!/bin/sh
# Policy: docs/threading/encoder.md
set -eux
echo '1..1'
set -v
status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "encoder/scheduling/0007_interrupted_light" || status=$?
test "$status" -ne 77 || {
    echo 'ok 1 # SKIP thread support unavailable'
    exit 0
}
test "$status" -eq 0 || {
    echo 'not ok 1 - Interrupted light observations do not expand'
    exit 0
}
echo 'ok 1 - Interrupted light observations do not expand'
exit 0
