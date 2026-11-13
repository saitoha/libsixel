#!/bin/sh
# Verify AWK TAP classification for todo word boundary.
set -eux
echo '1..1'
set -v

sh "$TOP_SRCDIR/tests/_support/tools/tap-directive.sh" \
    no todo_word_boundary FAIL || {
    echo 'not ok 1 - AWK todo word boundary classification'
    exit 0
}
echo 'ok 1 - AWK todo word boundary classification'
exit 0
