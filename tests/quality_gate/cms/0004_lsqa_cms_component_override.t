#!/bin/sh
# Policy: docs/loader/color-management/README.md
# Verify lsqa cms component override.
# The custom ICC fixture makes source interpretation observable.

set -eux

printf '1..1\n'
set -v

input="${TOP_SRCDIR}/tests/data/colormgmt/input/custom/rgb_parametric_012.png"
reference="${TOP_SRCDIR}/tests/data/colormgmt/reference/custom/rgb_parametric_012_png_builtin.six"

expected=$(${SIXEL_RUNTIME-} "${LSQA_PATH}" -m DE00_max -L "builtin!" --cms-engine=builtin "$input" "$reference") || {
    echo "not ok 1 - load enabled reference"
    exit 0
}
actual=$(${SIXEL_RUNTIME-} "${LSQA_PATH}" -m DE00_max -L "builtin:cms_engine=builtin!" --cms-engine=none "$input" "$reference") || {
    echo "not ok 1 - component override"
    exit 0
}
# Compare finite color-error values to observe CMS routing directly.
case "${actual}:${expected}" in
    *nan*|*inf*|:*)
        echo "not ok 1 - CMS comparison must produce finite values"
        exit 0
        ;;
esac
test "$actual" = "$expected" || {
    echo "not ok 1 - component CMS must override global choice"
    exit 0
}

echo "ok 1 - lsqa cms component override"
exit 0
