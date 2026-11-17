#!/bin/sh
# Regression: auto negative exposure; independent numerical expectations.
set -eux
echo '1..1'
set -v
result=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    hdr/0015_auto_negative_exposure || result=$?
test "$result" -ne 77 || {
    echo 'ok 1 # SKIP loader unavailable'
    exit 0
}
test "$result" -eq 0 || {
    echo 'not ok 1 - auto negative exposure'
    exit 0
}
echo 'ok 1 - auto negative exposure'
exit 0
