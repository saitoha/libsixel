#!/bin/sh
# Policy: docs/loader/color-management/README.md
# ColorSync curv gray above one passthrough preserves its value or native behavior contract.
# Every pixel has an outside source component; mixed pixels have separate tests.
set -eux
result=0
trace=$(SIXEL_TRACE_TOPIC=loader ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    cms/0147_colorsync_curv 11 generic 6 2>&1) || result=$?
test "$result" -ne 77 || {
    echo "1..0 # SKIP ColorSync is unavailable"
    set -v
    exit 0
}
echo "1..1"
set -v
test "$result" -eq 0 || {
    echo "$trace" >&2
    echo "not ok 1 - ColorSync curv gray above one passthrough"
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
case "${trace}" in *"LSXCMS2|event=source-profile-adapter|"*) false ;; *) true ;; esac || {
    echo "$trace" >&2
    echo "not ok 1 - unexpected source preparation path"
    exit 0
}
echo "ok 1 - ColorSync curv gray above one passthrough"
exit 0
