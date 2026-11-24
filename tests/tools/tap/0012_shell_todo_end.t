#!/bin/sh
# Verify shell TAP classification for todo end.
set -eux
echo '1..1'
set -v

"${SIXEL_TEST_CONFIG_SHELL}" "$TOP_SRCDIR/tests/_support/tools/tap-directive.sh" \
    yes todo_end XFAIL || {
    printf '\n%s\n' 'not ok 1 - shell todo end classification'
    exit 0
}
printf '\n%s\n' 'ok 1 - shell todo end classification'
exit 0
