#!/bin/sh
# Verify shell TAP classification for skip word boundary.
set -eux
echo '1..1'
set -v

"${SIXEL_TEST_CONFIG_SHELL}" "$TOP_SRCDIR/tests/_support/tools/tap-directive.sh" \
    yes skip_word_boundary PASS || {
    printf '\n%s\n' 'not ok 1 - shell skip word boundary classification'
    exit 0
}
printf '\n%s\n' 'ok 1 - shell skip word boundary classification'
exit 0
