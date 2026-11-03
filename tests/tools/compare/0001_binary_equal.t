#!/bin/sh
# Policy: docs/build/test-harness.md
# Policy: docs/testing/staticcheck.md
set -eux
echo '1..1'
set -v
status=0
printf '\000\012\015\032\177\200\377' | "$SIXEL_TEST_COMPARE" -s - "$TOP_SRCDIR/tests/tools/compare/data/octet.bin" || status=$?
test "$status" -eq 0 || {
    echo 'not ok 1 - binary octets including NUL and Ctrl-Z compare equal'
    exit 0
}
echo 'ok 1 - binary octets including NUL and Ctrl-Z compare equal'
exit 0
