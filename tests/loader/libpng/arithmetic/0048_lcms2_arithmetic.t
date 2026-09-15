#!/bin/sh
# Test-plan: docs/testing/png-chrm-arithmetic.md
# Policy: docs/loader/libpng.md
# Verify lcms2 selected cHRM arithmetic.
set -eux
test "${HAVE_LIBPNG-}" = 1 || {
    echo "1..0 # SKIP libpng is unavailable"
    exit 0
}
test "${HAVE_LCMS2-}" = 1 || {
    echo "1..0 # SKIP lcms2 is unavailable"
    exit 0
}
echo "1..1"
set -v

result=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/0210_png_arith_loader 5 "libpng:cms_engine=lcms2:cms_target=linear:prefer_8bit=0:chrm_arithmetic=auto!" 1 || result=$?
test "$result" -ne 77 || {
    echo "ok 1 # SKIP required arithmetic representation is unavailable"
    exit 0
}
test "$result" -eq 0 || {
    echo "not ok 1 - lcms2 selected cHRM arithmetic"
    exit 0
}
echo "ok 1 - lcms2 selected cHRM arithmetic"
exit 0
