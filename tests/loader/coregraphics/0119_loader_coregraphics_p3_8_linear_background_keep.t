#!/bin/sh
# Policy: docs/loader/coregraphics.md
# Verify p3 8 original alpha over linear background with keep.
set -eux
echo "1..1"
set -v

result=0
trace=$(SIXEL_TRACE_TOPIC=loader ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    loader/0187_coregraphics_png p3 8 rgba 0 2 2 colorsync \
    "${TOP_SRCDIR}/tests/data/color-numeric/png-p3-8-rgba.png" 2>&1) || result=$?
test "$result" -ne 77 || {
    echo "ok 1 # SKIP CoreGraphics is unavailable"
    exit 0
}
test "$result" -eq 0 || {
    echo "not ok 1 - p3 8 original alpha over linear background with keep"
    exit 0
}
test "${trace#*"LSXCMS2|event=apply|engine=colorsync|path=external|src=4|dst=4|ok=1"}" != "$trace" || {
    echo "$trace" >&2
    echo "not ok 1 - original RGB float did not use ColorSync"
    exit 0
}
test "${trace#*"LSXCMS2|event=apply|engine=builtin|"}" = "$trace" || {
    echo "not ok 1 - builtin substituted for the native source transform"
    exit 0
}
test "${trace#*"LSXCMS2|event=apply|engine=lcms2|"}" = "$trace" || {
    echo "not ok 1 - Little CMS substituted for the native source transform"
    exit 0
}
echo "ok 1 - p3 8 original alpha over linear background with keep"
exit 0
