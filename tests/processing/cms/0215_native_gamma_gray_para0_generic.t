#!/bin/sh
# Policy: docs/loader/color-management.md
# native gamma gray para0 generic compares original-tag native behavior on this OS.
set -eux
result=0
trace=$(SIXEL_TRACE_TOPIC=loader,loader_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    cms/0195_native_gamma gray 0 generic 3 0 colorsync 2>&1) || result=$?
test "$result" -ne 77 || {
    echo "1..0 # SKIP requested CMS engine is unavailable"
    set -v
    exit 0
}
echo "1..1"
set -v
test "$result" -eq 0 || {
    echo "$trace" >&2
    echo "not ok 1 - native gamma gray para0 generic"
    exit 0
}
case "${trace}" in *"LSXCMS2|event=apply|engine=colorsync|"*) true ;; *) false ;; esac || {
    echo "$trace" >&2
    echo "not ok 1 - requested engine did not apply"
    exit 0
}
case "${trace}" in *"to=original"*) true ;; *) false ;; esac || {
    echo "$trace" >&2
    echo "not ok 1 - unexpected original-curve branch"
    exit 0
}
expected=${trace#*'NGEXPECT|begin
'}
expected=${expected%%'NGEXPECT|end'*}
actual=${trace#*'NGPHASE|begin
'}
actual=${actual%%'NGPHASE|end'*}
{ test -n "$expected" && case "${actual}" in *"$expected"*) true ;; *) false ;; esac; } || {
    echo "$trace" >&2
    echo "not ok 1 - native TRC XYZ or sample bytes changed"
    exit 0
}
echo "ok 1 - native gamma gray para0 generic"
exit 0
