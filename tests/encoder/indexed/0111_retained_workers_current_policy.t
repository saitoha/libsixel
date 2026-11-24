#!/bin/sh
# Retained commit execution: workers current policy.
# Policy: docs/threading/encoder.md
set -eux
echo '1..1'
set -v
status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "encoder/indexed/0111_retained_workers_current_policy" || status=$?
test "$status" -ne 77 || {
    printf '\n%s\n' 'ok 1 # SKIP required thread support is unavailable'
    exit 0
}
test "$status" -eq 0 || {
    printf '\n%s\n' 'not ok 1 - retained workers current policy'
    exit 0
}
printf '\n%s\n' 'ok 1 - retained workers current policy'
exit 0
