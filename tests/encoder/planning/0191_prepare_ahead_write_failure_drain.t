#!/bin/sh
# Policy: docs/threading/animation.md
# Verify prepare ahead write failure drain.
set -eux
echo '1..1'
set -v
status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    encoder/planning/0191_prepare_ahead_write_failure_drain >/dev/null || status=$?
test "$status" -ne 77 || {
    echo 'ok 1 # SKIP required thread or GPU support unavailable'
    exit 0
}
test "$status" -eq 0 || {
    echo 'not ok 1 - prepare ahead write failure drain'
    exit 0
}
echo 'ok 1 - prepare ahead write failure drain'
exit 0
