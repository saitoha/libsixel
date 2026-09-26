#!/bin/sh
# Policy: docs/loader/color-management/README.md
# Verify lab linear profile lcms2.
set -eux
result=0
trace=$(SIXEL_TRACE_TOPIC=loader,loader_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    cms/0345_lab_linear_profile_lcms2 2>&1) || result=$?
test "$result" -ne 77 || {
    echo "1..0 # SKIP required loader or CMS engine is unavailable"
    set -v
    exit 0
}
echo "1..1"
set -v
test "$result" -eq 0 || {
    echo "$trace" >&2
    echo "not ok 1 - lab linear profile lcms2"
    exit 0
}
case "${trace}" in *"LSXCMS2|event=apply|engine=lcms2|"*) true ;; *) false ;; esac || {
    echo "$trace" >&2
    echo "not ok 1 - requested CMS did not apply"
    exit 0
}
case "${trace}" in *"|src=8|dst=4|ok=1"*) true ;; *) false ;; esac || {
    echo "$trace" >&2
    echo "not ok 1 - Lab to float RGB apply was not observed"
    exit 0
}
negative=${trace#*"TLINPROFILE|negative-begin"}
case "${negative}" in *"LSXCMS2|event=apply"*) false ;; *) true ;; esac || {
    echo "$trace" >&2
    echo "not ok 1 - mismatched profile reached CMS apply"
    exit 0
}
echo "ok 1 - lab linear profile lcms2"
exit 0
