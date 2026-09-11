#!/bin/sh
# Policy: docs/loader/color-management.md
# Verify lsqa cms reference selection.
# The custom ICC fixture makes source interpretation observable.

set -eux

printf '1..1\n'
set -v

input="${TOP_SRCDIR}/tests/data/colormgmt/input/custom/rgb_parametric_012.png"
reference="${TOP_SRCDIR}/tests/data/colormgmt/reference/custom/rgb_parametric_012_png_builtin.six"

expected=$(${SIXEL_RUNTIME-} "${LSQA_PATH}" -m MS-SSIM -L "builtin:cms_engine=builtin!" "$input" "$reference") || {
    echo "not ok 1 - load expected CMS reference"
    exit 0
}
actual=$(${SIXEL_RUNTIME-} "${LSQA_PATH}" -m MS-SSIM -L "builtin!" --cms-engine=builtin "$input" "$reference") || {
    echo "not ok 1 - explicit CMS reference"
    exit 0
}
disabled=$(${SIXEL_RUNTIME-} "${LSQA_PATH}" -m MS-SSIM -L "builtin!" --cms-engine=none "$input" "$reference") || {
    echo "not ok 1 - disabled CMS reference"
    exit 0
}
test "$actual" = "$expected" || {
    echo "not ok 1 - CMS must match component selection"
    exit 0
}
test "$actual" != "$disabled" || {
    echo "not ok 1 - CMS must change profiled reference interpretation"
    exit 0
}

echo "ok 1 - lsqa cms reference selection"
exit 0
