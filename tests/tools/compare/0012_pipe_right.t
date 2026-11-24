#!/bin/sh
# Policy: docs/build/test-harness.md
# Policy: docs/misc/platforms/openvms.md
# Coverage: OV-25
set -eux
echo '1..1'
set -v
# An external producer closes its pipe after the finite binary fixture.
# Redirection would use a regular file instead of exercising pipe EOF.
# shellcheck disable=SC2002
cat "$TOP_SRCDIR/tests/tools/compare/data/octet.bin" |
    "$SIXEL_TEST_COMPARE" -s "$TOP_SRCDIR/tests/tools/compare/data/octet.bin" - || {
    printf '\n%s\n' 'not ok 1 - binary pipe input terminates as the right operand'
    exit 0
}
printf '\n%s\n' 'ok 1 - binary pipe input terminates as the right operand'
exit 0
