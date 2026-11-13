#!/bin/sh
# Verify shell TAP classification for todo end.
set -eux
echo '1..1'
set -v

sh "$TOP_SRCDIR/tests/_support/tools/tap-directive.sh" \
    yes todo_end XFAIL || {
    echo 'not ok 1 - shell todo end classification'
    exit 0
}
echo 'ok 1 - shell todo end classification'
exit 0
