#!/bin/sh
# Regression: relative before cached byte; independent numerical expectations.
set -eux
echo '1..1'
set -v
result=0
SIXEL_LOADER_CMS_BPC=1 \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    bpc/0021_relative_before_cached_byte || result=$?
test "$result" -ne 77 || {
    echo 'ok 1 # SKIP loader unavailable'
    exit 0
}
test "$result" -eq 0 || {
    echo 'not ok 1 - relative before cached byte'
    exit 0
}
echo 'ok 1 - relative before cached byte'
exit 0
