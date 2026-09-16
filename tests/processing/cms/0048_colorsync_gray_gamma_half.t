#!/bin/sh
# Policy: docs/loader/color-management.md
# Verify original gray samples with gamma half through ColorSync.
set -eux
echo "1..1"
set -v

result=0
trace=$(SIXEL_TRACE_TOPIC=loader ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    cms/0045_linear_gray_samples colorsync 0 2>&1) || result=$?
test "$result" -ne 77 || {
    echo "ok 1 # SKIP requested CMS engine is unavailable"
    exit 0
}
test "$result" -eq 0 || {
    echo "$trace" >&2
    echo "not ok 1 - colorsync original gray samples to linear RGB"
    exit 0
}
test "${trace#*"LSXCMS2|event=source-profile-adapter|engine=colorsync|path=gray-pure-gamma|tag=kTRC|from=para0|to=para3"}" != "$trace" || {
    echo "not ok 1 - original pure-gamma profile was not adapted"
    exit 0
}
test "${trace#*"LSXCMS2|event=apply|engine=colorsync|path=external|src=9|dst=4|ok=1"}" != "$trace" || {
    echo "not ok 1 - requested CMS did not apply to packed gray float"
    exit 0
}
echo "ok 1 - colorsync original gray samples to linear RGB"
exit 0
