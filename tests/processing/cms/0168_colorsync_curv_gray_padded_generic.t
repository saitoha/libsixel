#!/bin/sh
# Policy: docs/loader/color-management/README.md
# ColorSync curv gray padded generic preserves its value or native behavior contract.
set -eux
result=0
trace=$(SIXEL_TRACE_TOPIC=loader ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    cms/0147_colorsync_curv 13 generic 0 2>&1) || result=$?
test "$result" -ne 77 || {
    echo "1..0 # SKIP ColorSync is unavailable"
    set -v
    exit 0
}
echo "1..1"
set -v
test "$result" -eq 0 || {
    echo "$trace" >&2
    echo "not ok 1 - ColorSync curv gray padded generic"
    exit 0
}
case "${trace}" in *"LSXCMS2|event=apply|engine=colorsync|path=external|"*) true ;; *) false ;; esac || {
    echo "$trace" >&2
    echo "not ok 1 - native ColorSync did not apply"
    exit 0
}
case "${trace}" in *"LSXCMS2|event=apply|engine=builtin|"*) false ;; *) true ;; esac || {
    echo "not ok 1 - builtin substituted for ColorSync"
    exit 0
}
case "${trace}" in *"LSXCMS2|event=apply|engine=lcms2|"*) false ;; *) true ;; esac || {
    echo "not ok 1 - Little CMS substituted for ColorSync"
    exit 0
}
case "${trace}" in *"LSXCMS2|event=source-profile-adapter|"*) true ;; *) false ;; esac || {
    echo "$trace" >&2
    echo "not ok 1 - unexpected source preparation path"
    exit 0
}
echo "ok 1 - ColorSync curv gray padded generic"
exit 0
