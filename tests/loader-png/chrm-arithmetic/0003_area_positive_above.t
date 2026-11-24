#!/bin/sh
# Test-plan: docs/testing/png-chrm-arithmetic.md
# Policy: docs/loader/libpng.md
# Verify area positive above.
set -eux
echo "1..1"
set -v

result=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/0203_png_arith_dispatch 2 0 || result=$?
test "$result" -ne 77 || {
    printf '\n%s\n' "ok 1 # SKIP required arithmetic representation is unavailable"
    exit 0
}
test "$result" -eq 0 || {
    printf '\n%s\n' "not ok 1 - area positive above"
    exit 0
}
printf '\n%s\n' "ok 1 - area positive above"
exit 0
