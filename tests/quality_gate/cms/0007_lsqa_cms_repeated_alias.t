#!/bin/sh
# Policy: docs/loader/color-management.md
# Verify lsqa cms repeated alias.
# The custom ICC fixture makes source interpretation observable.

set -eux

printf '1..1\n'
set -v

input="${TOP_SRCDIR}/tests/data/colormgmt/input/custom/rgb_parametric_012.png"
reference="${TOP_SRCDIR}/tests/data/colormgmt/reference/custom/rgb_parametric_012_png_builtin.six"

expected=$(${SIXEL_RUNTIME-} "${LSQA_PATH}" -m MS-SSIM -L "builtin!" --cms-engine=none "$input" "$reference") || {
    echo "not ok 1 - load disabled reference"
    exit 0
}
actual=$(${SIXEL_RUNTIME-} "${LSQA_PATH}" -m MS-SSIM -L "builtin!" --cms-engine=builtin -# off "$input" "$reference") || {
    echo "not ok 1 - repeated CMS alias"
    exit 0
}
test "$actual" = "$expected" || {
    echo "not ok 1 - last CMS selection must win"
    exit 0
}

echo "ok 1 - lsqa cms repeated alias"
exit 0
