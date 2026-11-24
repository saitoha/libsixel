#!/bin/sh
# Verify AWK TAP classification for skip end.
set -eux
echo '1..1'
set -v

"${SIXEL_TEST_CONFIG_SHELL}" "$TOP_SRCDIR/tests/_support/tools/tap-directive.sh" \
    no skip_end SKIP || {
    printf '\n%s\n' 'not ok 1 - AWK skip end classification'
    exit 0
}
printf '\n%s\n' 'ok 1 - AWK skip end classification'
exit 0
