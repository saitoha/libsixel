#!/bin/sh
# Test-plan: docs/testing/png-chrm-arithmetic.md
# Policy: docs/loader/libpng.md
# Verify ordinary auto p3 independent numeric budget.
set -eux
echo "1..1"
set -v

result=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/0205_png_arith_double 1 0 || result=$?
test "$result" -ne 77 || {
    echo "ok 1 # SKIP required arithmetic representation is unavailable"
    exit 0
}
test "$result" -eq 0 || {
    echo "not ok 1 - ordinary auto p3 independent numeric budget"
    exit 0
}
echo "ok 1 - ordinary auto p3 independent numeric budget"
exit 0
