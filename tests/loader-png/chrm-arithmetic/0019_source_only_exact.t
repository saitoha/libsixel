#!/bin/sh
# Test-plan: docs/testing/png-chrm-arithmetic.md
# Verify source only exact.
set -eux
echo "1..1"
set -v

result=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/0204_png_arith_exact 2 2 0 || result=$?
test "$result" -ne 77 || {
    echo "ok 1 # SKIP required arithmetic representation is unavailable"
    exit 0
}
test "$result" -eq 0 || {
    echo "not ok 1 - source only exact"
    exit 0
}
echo "ok 1 - source only exact"
exit 0
