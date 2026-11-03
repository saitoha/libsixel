#!/bin/sh
# Policy: docs/build/test-harness.md
# Policy: docs/testing/staticcheck.md
# Policy: docs/misc/platforms/openvms.md
# Coverage: OV-25
set -eux
echo '1..1'
set -v
status=0
# Both operands are read-only, including the redirected descriptor.
# shellcheck disable=SC2094
"$SIXEL_TEST_COMPARE" -s "$TOP_SRCDIR/tests/tools/compare/data/octet.bin" - < "$TOP_SRCDIR/tests/tools/compare/data/octet.bin" || status=$?
test "$status" -eq 0 || {
    echo 'not ok 1 - standard input is accepted as the right operand'
    exit 0
}
echo 'ok 1 - standard input is accepted as the right operand'
exit 0
