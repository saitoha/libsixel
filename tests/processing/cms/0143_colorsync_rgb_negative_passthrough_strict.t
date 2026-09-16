#!/bin/sh
# Policy: docs/loader/color-management.md
# Verify ColorSync negative passthrough strict against the original native transform.
set -eux
result=0
trace=$(SIXEL_TRACE_TOPIC=loader ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    cms/0130_colorsync_rgb_passthrough 6 strict 2>&1) || result=$?
test "$result" -ne 77 || {
    echo "1..0 # SKIP ColorSync is unavailable"
    set -v
    exit 0
}
echo "1..1"
set -v
test "$result" -eq 0 || {
    echo "$trace" >&2
    echo "not ok 1 - ColorSync negative passthrough strict"
    exit 0
}
test "${trace#*"LSXCMS2|event=apply|engine=colorsync|path=external|src=4|dst=4|ok=1"}" != "$trace" || {
    echo "$trace" >&2
    echo "not ok 1 - original RGB float did not use ColorSync"
    exit 0
}
test "${trace#*"LSXCMS2|event=apply|engine=builtin|"}" = "$trace" || {
    echo "not ok 1 - builtin substituted for ColorSync"
    exit 0
}
test "${trace#*"LSXCMS2|event=apply|engine=lcms2|"}" = "$trace" || {
    echo "not ok 1 - Little CMS substituted for ColorSync"
    exit 0
}
test "${trace#*"LSXCMS2|event=source-profile-adapter|engine=colorsync|path=rgb-pure-gamma"}" = "$trace" || {
    echo "not ok 1 - excluded native source was prepared"
    exit 0
}
echo "ok 1 - ColorSync negative passthrough strict"
exit 0
