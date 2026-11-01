#!/bin/sh
# Test-plan: docs/testing/png-chrm-arithmetic.md
# Verify invalid pixels separate.
set -eux
echo "1..1"
set -v

result=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/0209_png_arith_invalid 0 || result=$?
test "$result" -ne 77 || {
    echo "ok 1 # SKIP required arithmetic representation is unavailable"
    exit 0
}
test "$result" -eq 0 || {
    echo "not ok 1 - invalid pixels separate"
    exit 0
}
echo "ok 1 - invalid pixels separate"
exit 0
