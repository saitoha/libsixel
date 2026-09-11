#!/bin/sh
# Policy: docs/loader/color-management.md
# Verify lsqa cms omitted uses env.
# The custom ICC fixture makes source interpretation observable.

set -eux

printf '1..1\n'
set -v

input="${TOP_SRCDIR}/tests/data/colormgmt/input/custom/rgb_parametric_012.png"
reference="${TOP_SRCDIR}/tests/data/colormgmt/reference/custom/rgb_parametric_012_png_builtin.six"

expected=$(${SIXEL_RUNTIME-} "${LSQA_PATH}" -m MS-SSIM -L "builtin:cms_engine=builtin!" "$input" "$reference") || {
    echo "not ok 1 - load enabled reference"
    exit 0
}
actual=$(${SIXEL_RUNTIME-} "${LSQA_PATH}" -m MS-SSIM -L "builtin!" --env SIXEL_LOADER_CMS_ENGINE=builtin "$input" "$reference") || {
    echo "not ok 1 - inherited CMS default"
    exit 0
}
test "$actual" = "$expected" || {
    echo "not ok 1 - omitted CMS option must preserve environment"
    exit 0
}

echo "ok 1 - lsqa cms omitted uses env"
exit 0
