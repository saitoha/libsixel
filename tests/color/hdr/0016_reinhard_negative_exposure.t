#!/bin/sh
# Regression: reinhard negative exposure; independent numerical expectations.
set -eux
echo '1..1'
set -v
result=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    hdr/0016_reinhard_negative_exposure || result=$?
test "$result" -ne 77 || {
    printf '\n%s\n' 'ok 1 # SKIP loader unavailable'
    exit 0
}
test "$result" -eq 0 || {
    printf '\n%s\n' 'not ok 1 - reinhard negative exposure'
    exit 0
}
printf '\n%s\n' 'ok 1 - reinhard negative exposure'
exit 0
