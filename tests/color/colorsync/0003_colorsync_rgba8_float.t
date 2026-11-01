#!/bin/sh
# Policy: docs/loader/color-management/README.md
# Verify original integer input and actual colorsync rgba8 float conversion.
set -eux
echo "1..1"
set -v

status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "cms/0025_integer_rgb_float" colorsync 4 separate || status=$?
test "$status" -ne 77 || {
    echo "ok 1 - colorsync rgba8 float # SKIP colorsync unavailable"
    exit 0
}
test "$status" -eq 0 || {
    echo "not ok 1 - colorsync rgba8 float"
    exit 0
}
echo "ok 1 - colorsync rgba8 float"
exit 0
