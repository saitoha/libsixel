#!/bin/sh
# Verify AWK TAP classification for skip reason.
set -eux
echo '1..1'
set -v

sh "$TOP_SRCDIR/tests/_support/tools/tap-directive.sh" \
    no skip_reason SKIP || {
    echo 'not ok 1 - AWK skip reason classification'
    exit 0
}
echo 'ok 1 - AWK skip reason classification'
exit 0
