#!/bin/sh
# Policy: docs/loader/color-management.md
# Verify float white be.
set -eux
result=0
trace=$(SIXEL_TRACE_TOPIC=loader,loader_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    loader/0232_float_white_be \
    "${TOP_SRCDIR}/tests/data/color-numeric/tiff-float-white-be.tif" 2>&1) || result=$?
test "$result" -ne 77 || {
    echo "1..0 # SKIP libtiff is unavailable"
    set -v
    exit 0
}
echo "1..1"
set -v
test "$result" -eq 0 || {
    echo "$trace" >&2
    echo "not ok 1 - float white be"
    exit 0
}
test "${trace#*"TFLOAT|control=1|case-begin"}" != "$trace" || {
    echo "$trace" >&2
    echo "not ok 1 - successful control marker is absent"
    exit 0
}
control=${trace%%"TFLOAT|control=1|case-begin"*}
test "${control#*"LSXCMS2|event=apply|engine=builtin|"}" != "$control" || {
    echo "$trace" >&2
    echo "not ok 1 - positive CMS trace control did not apply"
    exit 0
}
test "${control#*"|ok=1"}" != "$control" || {
    echo "$trace" >&2
    echo "not ok 1 - positive CMS trace control was unsuccessful"
    exit 0
}
observed=${trace#*"TFLOAT|control=1|case-begin"}
test "$observed" = "${observed#*"LSXCMS2|event=apply"}" || {
    echo "$trace" >&2
    echo "not ok 1 - unexpected CMS application"
    exit 0
}
test "${observed#*"LSXLOAD1|event=ok|loader=libtiff|"}" != "$observed" || {
    echo "$trace" >&2
    echo "not ok 1 - libtiff did not deliver the frame"
    exit 0
}
echo "ok 1 - float white be"
exit 0
