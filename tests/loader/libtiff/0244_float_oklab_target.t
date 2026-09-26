#!/bin/sh
# Policy: docs/loader/color-management/README.md
# Verify float oklab target.
set -eux
result=0
trace=$(SIXEL_TRACE_TOPIC=loader,loader_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    loader/0244_float_oklab_target \
    "${TOP_SRCDIR}/tests/data/color-numeric/tiff-float-rgb-le.tif" 2>&1) || result=$?
test "$result" -ne 77 || {
    echo "1..0 # SKIP libtiff is unavailable"
    set -v
    exit 0
}
echo "1..1"
set -v
test "$result" -eq 0 || {
    echo "$trace" >&2
    echo "not ok 1 - float oklab target"
    exit 0
}
case "${trace}" in *"TFLOAT|control=1|case-begin"*) true ;; *) false ;; esac || {
    echo "$trace" >&2
    echo "not ok 1 - successful control marker is absent"
    exit 0
}
control=${trace%%"TFLOAT|control=1|case-begin"*}
case "${control}" in *"LSXCMS2|event=apply|engine=builtin|"*) true ;; *) false ;; esac || {
    echo "$trace" >&2
    echo "not ok 1 - positive CMS trace control did not apply"
    exit 0
}
case "${control}" in *"|ok=1"*) true ;; *) false ;; esac || {
    echo "$trace" >&2
    echo "not ok 1 - positive CMS trace control was unsuccessful"
    exit 0
}
observed=${trace#*"TFLOAT|control=1|case-begin"}
case "${observed}" in *"LSXCMS2|event=apply"*) false ;; *) true ;; esac || {
    echo "$trace" >&2
    echo "not ok 1 - unexpected CMS application"
    exit 0
}
case "${observed}" in *"LSXLOAD1|event=ok|loader=libtiff|"*) true ;; *) false ;; esac || {
    echo "$trace" >&2
    echo "not ok 1 - libtiff did not deliver the frame"
    exit 0
}
echo "ok 1 - float oklab target"
exit 0
