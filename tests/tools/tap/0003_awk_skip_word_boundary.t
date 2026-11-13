#!/bin/sh
# Verify AWK TAP classification for skip word boundary.
set -eux
echo '1..1'
set -v

sh "$TOP_SRCDIR/tests/_support/tools/tap-directive.sh" \
    no skip_word_boundary PASS || {
    echo 'not ok 1 - AWK skip word boundary classification'
    exit 0
}
echo 'ok 1 - AWK skip word boundary classification'
exit 0
