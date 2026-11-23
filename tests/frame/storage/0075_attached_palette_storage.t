#!/bin/sh
set -eux
echo '1..1'
set -v

status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    frame/0014_clone_float_canonical attached-palette-storage || status=$?
test "$status" -ne 23 || {
    echo 'not ok 1 - An attached implicit palette retains its extent after reshaping # TODO attachment does not record the extent'
    exit 0
}
test "$status" -ne 0 || {
    echo 'not ok 1 - Reproducer now passes. Remove the expected failure.'
    exit 0
}
echo 'not ok 1 - Unexpected fixture or test result'
exit 0
