#!/bin/sh
# Verify AWK TAP classification for todo reason.
set -eux
echo '1..1'
set -v

"${SIXEL_TEST_CONFIG_SHELL}" "$TOP_SRCDIR/tests/_support/tools/tap-directive.sh" \
    no todo_reason XFAIL || {
    printf '\n%s\n' 'not ok 1 - AWK todo reason classification'
    exit 0
}
printf '\n%s\n' 'ok 1 - AWK todo reason classification'
exit 0
