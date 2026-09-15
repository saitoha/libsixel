#!/bin/sh
# Test-plan: docs/testing/png-chrm-arithmetic.md
# Policy: docs/loader/libpng.md
# Verify p3_d50 auto nonneutral tiny exact bits.
set -eux
echo "1..1"
set -v

result=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/0212_png_arith_tiny 2 0 || result=$?
test "$result" -ne 77 || {
    echo "ok 1 # SKIP required arithmetic representation is unavailable"
    exit 0
}
test "$result" -eq 0 || {
    echo "not ok 1 - p3_d50 auto nonneutral tiny exact bits"
    exit 0
}
echo "ok 1 - p3_d50 auto nonneutral tiny exact bits"
exit 0
