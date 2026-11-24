#!/bin/sh
# Policy: docs/loader/color-management/README.md
# Verify lsqa cms cli over env.
# The custom ICC fixture makes source interpretation observable.

set -eux

printf '1..1\n'
set -v

input="${TOP_SRCDIR}/tests/data/colormgmt/input/custom/rgb_parametric_012.png"
reference="${TOP_SRCDIR}/tests/data/colormgmt/reference/custom/rgb_parametric_012_png_builtin.six"

expected=$(${SIXEL_RUNTIME-} "${LSQA_PATH}" -m DE00_max -L "builtin:cms_engine=none!" "$input" "$reference") || {
    printf '\n%s\n' "not ok 1 - load disabled reference"
    exit 0
}
actual=$(${SIXEL_RUNTIME-} "${LSQA_PATH}" -m DE00_max -L "builtin!" --cms-engine=none --env SIXEL_LOADER_CMS_ENGINE=builtin "$input" "$reference") || {
    printf '\n%s\n' "not ok 1 - CMS CLI overrides env"
    exit 0
}
# Compare finite color-error values to observe CMS routing directly.
case "${actual}:${expected}" in
    *nan*|*inf*|:*)
        printf '\n%s\n' "not ok 1 - CMS comparison must produce finite values"
        exit 0
        ;;
esac
test "$actual" = "$expected" || {
    printf '\n%s\n' "not ok 1 - CLI must override later global env"
    exit 0
}

printf '\n%s\n' "ok 1 - lsqa cms cli over env"
exit 0
