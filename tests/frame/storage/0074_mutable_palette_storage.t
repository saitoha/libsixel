#!/bin/sh
set -eux
echo '1..1'
set -v

status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    frame/0014_clone_float_canonical mutable-palette-storage || status=$?
test "$status" -ne 23 || {
    printf '\n%s\n' 'not ok 1 - Mutable indices increase the reported palette extent # TODO initial extent hides later indices'
    exit 0
}
test "$status" -ne 0 || {
    printf '\n%s\n' 'not ok 1 - Reproducer now passes. Remove the expected failure.'
    exit 0
}
printf '\n%s\n' 'not ok 1 - Unexpected fixture or test result'
exit 0
