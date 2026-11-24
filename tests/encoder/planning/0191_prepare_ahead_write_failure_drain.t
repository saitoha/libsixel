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
    printf '\n%s\n' 'ok 1 # SKIP preparation overlap unavailable for this build'
    exit 0
}
test "$status" -eq 0 || {
    printf '\n%s\n' 'not ok 1 - prepare ahead write failure drain'
    exit 0
}
printf '\n%s\n' 'ok 1 - prepare ahead write failure drain'
exit 0
