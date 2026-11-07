#!/bin/sh
# Policy: docs/build/test-harness.md
# Policy: docs/misc/platforms/openvms.md
# Coverage: OV-25
set -eux
echo '1..1'
set -v
# An external producer closes its pipe after the finite binary fixture.
cat "$TOP_SRCDIR/tests/tools/compare/data/octet.bin" |
    "$SIXEL_TEST_COMPARE" -s "$TOP_SRCDIR/tests/tools/compare/data/octet.bin" - || {
    echo 'not ok 1 - binary pipe input terminates as the right operand'
    exit 0
}
echo 'ok 1 - binary pipe input terminates as the right operand'
exit 0
