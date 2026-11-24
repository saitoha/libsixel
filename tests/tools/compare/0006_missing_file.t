#!/bin/sh
# Policy: docs/build/test-harness.md
# Policy: docs/testing/staticcheck.md
# Policy: docs/misc/platforms/openvms.md
# Coverage: OV-25
set -eux
echo '1..1'
set -v
status=0
 "$SIXEL_TEST_COMPARE" -s "$TOP_SRCDIR/tests/tools/compare/data/missing" "$TOP_SRCDIR/tests/tools/compare/data/empty" || status=$?
test "$status" -eq 2 || {
    printf '\n%s\n' 'not ok 1 - a missing input is an error rather than equality'
    exit 0
}
printf '\n%s\n' 'ok 1 - a missing input is an error rather than equality'
exit 0
