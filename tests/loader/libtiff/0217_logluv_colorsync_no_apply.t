#!/bin/sh
# Policy: docs/loader/color-management.md
# Verify logluv colorsync no apply.
set -eux
result=0
trace=$(SIXEL_TRACE_TOPIC=loader,loader_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    loader/0217_logluv_colorsync_no_apply \
    "${TOP_SRCDIR}/tests/data/color-numeric/tiff-logluv32.tif" 2>&1) || result=$?
test "$result" -ne 77 || {
    echo "1..0 # SKIP required loader or CMS engine is unavailable"
    set -v
    exit 0
}
echo "1..1"
set -v
test "$result" -eq 0 || {
    echo "$trace" >&2
    echo "not ok 1 - logluv colorsync no apply"
    exit 0
}
test "${trace#*"TLOGLUV|native-begin"}" != "$trace" || {
    echo "$trace" >&2
    echo "not ok 1 - native route marker is absent"
    exit 0
}
control=${trace%%"TLOGLUV|native-begin"*}
test "${control#*"LSXCMS2|event=apply|engine=builtin|"}" != "$control" || {
    echo "$trace" >&2
    echo "not ok 1 - positive CMS trace control did not apply"
    exit 0
}
test "${control#*"|ok=1"}" != "$control" || {
    echo "$trace" >&2
    echo "not ok 1 - positive CMS control was unsuccessful"
    exit 0
}
native=${trace#*"TLOGLUV|native-begin"}
test "$native" = "${native#*"LSXCMS2|event=apply"}" || {
    echo "$trace" >&2
    echo "not ok 1 - native XYZ was reinterpreted by CMS"
    exit 0
}
test "${native#*"LSXLOAD1|event=ok|loader=libtiff|"}" != "$native" || {
    echo "$trace" >&2
    echo "not ok 1 - libtiff did not deliver the frame"
    exit 0
}
echo "ok 1 - logluv colorsync no apply"
exit 0
