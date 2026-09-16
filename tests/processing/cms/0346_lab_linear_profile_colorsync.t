#!/bin/sh
# Policy: docs/loader/color-management.md
# Verify lab linear profile colorsync.
set -eux
result=0
trace=$(SIXEL_TRACE_TOPIC=loader,loader_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    cms/0346_lab_linear_profile_colorsync 2>&1) || result=$?
test "$result" -ne 77 || {
    echo "1..0 # SKIP required loader or CMS engine is unavailable"
    set -v
    exit 0
}
echo "1..1"
set -v
test "$result" -eq 0 || {
    echo "$trace" >&2
    echo "not ok 1 - lab linear profile colorsync"
    exit 0
}
test "${trace#*"LSXCMS2|event=apply|engine=colorsync|"}" != "$trace" || {
    echo "$trace" >&2
    echo "not ok 1 - requested CMS did not apply"
    exit 0
}
test "${trace#*"|src=8|dst=4|ok=1"}" != "$trace" || {
    echo "$trace" >&2
    echo "not ok 1 - Lab to float RGB apply was not observed"
    exit 0
}
negative=${trace#*"TLINPROFILE|negative-begin"}
test "$negative" = "${negative#*"LSXCMS2|event=apply"}" || {
    echo "$trace" >&2
    echo "not ok 1 - mismatched profile reached CMS apply"
    exit 0
}
echo "ok 1 - lab linear profile colorsync"
exit 0
