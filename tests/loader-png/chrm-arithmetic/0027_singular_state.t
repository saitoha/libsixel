#!/bin/sh
# Test-plan: docs/testing/png-chrm-arithmetic.md
# Verify singular state.
set -eux
echo "1..1"
set -v

result=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/0208_png_arith_singular  || result=$?
test "$result" -ne 77 || {
    printf '\n%s\n' "ok 1 # SKIP required arithmetic representation is unavailable"
    exit 0
}
test "$result" -eq 0 || {
    printf '\n%s\n' "not ok 1 - singular state"
    exit 0
}
printf '\n%s\n' "ok 1 - singular state"
exit 0
