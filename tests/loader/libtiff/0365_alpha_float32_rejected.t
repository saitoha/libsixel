#!/bin/sh
# Policy: docs/loader/libtiff.md
# Policy: docs/loader/color-management.md
# Preserve the six IEEEFP32-alpha rejection outcomes without a callback.
set -eux
result=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/0365_alpha_float32_rejected \
    "${TOP_SRCDIR}/tests/data/color-numeric" || result=$?
test "$result" -ne 77 || {
    echo "1..0 # SKIP libtiff is unavailable"
    set -v
    exit 0
}
echo "1..1"
set -v
test "$result" -eq 0 || {
    echo "not ok 1 - IEEEFP32 alpha rejected without callback"
    exit 0
}
echo "ok 1 - IEEEFP32 alpha rejected without callback"
exit 0
