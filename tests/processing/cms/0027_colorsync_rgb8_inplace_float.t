#!/bin/sh
# Policy: docs/loader/color-management.md
# Verify original integer input and actual colorsync rgb8 inplace float conversion.
set -eux
echo "1..1"
set -v

status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "cms/0025_integer_rgb_float" colorsync 3 alias || status=$?
test "$status" -ne 77 || {
    echo "ok 1 - colorsync rgb8 inplace float # SKIP colorsync unavailable"
    exit 0
}
test "$status" -eq 0 || {
    echo "not ok 1 - colorsync rgb8 inplace float"
    exit 0
}
echo "ok 1 - colorsync rgb8 inplace float"
exit 0
