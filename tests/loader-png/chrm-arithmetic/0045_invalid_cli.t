#!/bin/sh
# Test-plan: docs/testing/png-chrm-arithmetic.md
# Policy: docs/loader/libpng.md
# Verify invalid cli.
set -eux
test "${HAVE_LIBPNG-}" = 1 || {
    echo "1..0 # SKIP libpng is unavailable"
    exit 0
}
test "${HAVE_IMG2SIXEL-}" = 1 || {
    echo "1..0 # SKIP img2sixel is unavailable"
    exit 0
}
echo "1..1"
set -v

status=0
${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" "-Llibpng:chrm_arithmetic=invalid!" \
    "${TOP_SRCDIR}/tests/data/color-numeric/png-arithmetic-1.png" \
    >/dev/null 2>&1 || status=$?
test "$status" -ne 0 || {
    echo "not ok 1 - invalid cHRM arithmetic assignment was accepted"
    exit 0
}
echo "ok 1 - invalid cHRM arithmetic assignment is rejected"
exit 0
