#!/bin/sh
# Policy: docs/threading/encoder.md
set -eux
echo '1..1'
set -v
status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "encoder/scheduling/0015_tail_one" || status=$?
test "$status" -ne 77 || {
    printf '\n%s\n' 'ok 1 # SKIP thread support unavailable'
    exit 0
}
test "$status" -eq 0 || {
    printf '\n%s\n' 'not ok 1 - Three remaining bands use single-band jobs'
    exit 0
}
printf '\n%s\n' 'ok 1 - Three remaining bands use single-band jobs'
exit 0
