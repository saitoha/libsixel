#!/bin/sh
set -eux
echo '1..1'
set -v

status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    frame/0014_clone_float_canonical repeated-implicit-palette-init || status=$?
test "$status" -ne 77 || {
    printf '\n%s\n' 'ok 1 # SKIP CPU timer unavailable or too coarse'
    exit 0
}
test "$status" -ne 23 || {
    printf '\n%s\n' 'not ok 1 - Repeated PAL8 initialization avoids a full scan # TODO implicit palette is scanned on each initialization'
    exit 0
}
test "$status" -ne 0 || {
    printf '\n%s\n' 'not ok 1 - Reproducer now passes. Remove the expected failure.'
    exit 0
}
printf '\n%s\n' 'not ok 1 - Unexpected fixture or test result'
exit 0
