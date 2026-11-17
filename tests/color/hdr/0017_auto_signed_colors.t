#!/bin/sh
# Regression: auto signed colors; independent numerical expectations.
set -eux
echo '1..1'
set -v
result=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    hdr/0017_auto_signed_colors || result=$?
test "$result" -ne 77 || {
    echo 'ok 1 # SKIP loader unavailable'
    exit 0
}
test "$result" -eq 0 || {
    echo 'not ok 1 - auto signed colors'
    exit 0
}
echo 'ok 1 - auto signed colors'
exit 0
