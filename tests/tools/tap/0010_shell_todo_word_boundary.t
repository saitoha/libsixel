#!/bin/sh
# Verify shell TAP classification for todo word boundary.
set -eux
echo '1..1'
set -v

"${SIXEL_TEST_CONFIG_SHELL}" "$TOP_SRCDIR/tests/_support/tools/tap-directive.sh" \
    yes todo_word_boundary FAIL || {
    echo 'not ok 1 - shell todo word boundary classification'
    exit 0
}
echo 'ok 1 - shell todo word boundary classification'
exit 0
