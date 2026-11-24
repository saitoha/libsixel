#!/bin/sh
# Policy: docs/threading/encoder.md
set -eux
echo '1..1'
set -v
status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "encoder/scheduling/0002_light_once" || status=$?
test "$status" -ne 77 || {
    printf '\n%s\n' 'ok 1 # SKIP thread support unavailable'
    exit 0
}
test "$status" -eq 0 || {
    printf '\n%s\n' 'not ok 1 - One light frame does not expand admission'
    exit 0
}
printf '\n%s\n' 'ok 1 - One light frame does not expand admission'
exit 0
