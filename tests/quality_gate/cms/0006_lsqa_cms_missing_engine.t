#!/bin/sh
# Policy: docs/loader/color-management.md
# Verify lsqa cms missing engine.
# The custom ICC fixture makes source interpretation observable.

set -eux

printf '1..1\n'
set -v

input="${TOP_SRCDIR}/tests/data/colormgmt/input/custom/rgb_parametric_012.png"
reference="${TOP_SRCDIR}/tests/data/colormgmt/reference/custom/rgb_parametric_012_png_builtin.six"

status=0
message=$(${SIXEL_RUNTIME-} "${LSQA_PATH}" -m MS-SSIM "$input" "$reference" --cms-engine 2>&1 1>/dev/null) || status=$?
test "$status" -eq 2 || {
    echo "not ok 1 - missing engine exit status"
    exit 0
}
test "${message#*missing required argument*}" != "$message" || {
    echo "not ok 1 - missing engine diagnostic"
    exit 0
}

echo "ok 1 - lsqa cms missing engine"
exit 0
