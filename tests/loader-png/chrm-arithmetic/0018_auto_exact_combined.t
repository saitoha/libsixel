#!/bin/sh
# Test-plan: docs/testing/png-chrm-arithmetic.md
# Policy: docs/loader/libpng.md
# Verify auto exact combined.
set -eux
echo "1..1"
set -v

result=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/0204_png_arith_exact 5 0 1 || result=$?
test "$result" -ne 77 || {
    echo "ok 1 # SKIP required arithmetic representation is unavailable"
    exit 0
}
test "$result" -eq 0 || {
    echo "not ok 1 - auto exact combined"
    exit 0
}
echo "ok 1 - auto exact combined"
exit 0
