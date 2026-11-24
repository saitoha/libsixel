#!/bin/sh
# Test-plan: docs/testing/png-chrm-arithmetic.md
# Policy: docs/loader/libpng.md
# Verify selected 512bit unavailable remains terminal.
set -eux
test "${HAVE_LIBPNG-}" = 1 || {
    echo "1..0 # SKIP libpng is unavailable"
    exit 0
}
echo "1..1"
set -v

result=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/0211_png_arith_unavailable || result=$?
test "$result" -ne 77 || {
    printf '\n%s\n' "ok 1 # SKIP required arithmetic representation is unavailable"
    exit 0
}
test "$result" -eq 0 || {
    printf '\n%s\n' "not ok 1 - selected 512bit unavailable remains terminal"
    exit 0
}
printf '\n%s\n' "ok 1 - selected 512bit unavailable remains terminal"
exit 0
