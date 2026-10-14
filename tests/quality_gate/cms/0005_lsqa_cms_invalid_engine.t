#!/bin/sh
# Policy: docs/loader/color-management/README.md
# Verify lsqa cms invalid engine.
# The custom ICC fixture makes source interpretation observable.

set -eux

printf '1..1\n'
set -v

input="${TOP_SRCDIR}/tests/data/colormgmt/input/custom/rgb_parametric_012.png"
reference="${TOP_SRCDIR}/tests/data/colormgmt/reference/custom/rgb_parametric_012_png_builtin.six"

status=0
message=$(${SIXEL_RUNTIME-} "${LSQA_PATH}" -m DE00_max --cms-engine=unknown "$input" "$reference" 2>&1 1>/dev/null) || status=$?
test "$status" -eq 2 || {
    echo "not ok 1 - invalid engine exit status"
    exit 0
}
case "${message}" in *invalid\ argument\ for**) true ;; *) false ;; esac || {
    echo "not ok 1 - invalid engine diagnostic"
    exit 0
}

echo "ok 1 - lsqa cms invalid engine"
exit 0
