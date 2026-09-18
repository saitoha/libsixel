#!/bin/sh
# Policy: docs/loader/color-management.md
# native gamma para0 green swap generic compares original-tag native behavior on this OS.
set -eux
result=0
trace=$(SIXEL_TRACE_TOPIC=loader,loader_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    cms/0195_native_gamma power 15 generic 1 0 colorsync 2>&1) || result=$?
test "$result" -ne 77 || {
    echo "1..0 # SKIP requested CMS engine is unavailable"
    set -v
    exit 0
}
echo "1..1"
set -v
test "$result" -eq 0 || {
    echo "$trace" >&2
    echo "not ok 1 - native gamma para0 green swap generic"
    exit 0
}
case "${trace}" in *"LSXCMS2|event=apply|engine=colorsync|"*) true ;; *) false ;; esac || {
    echo "$trace" >&2
    echo "not ok 1 - requested engine did not apply"
    exit 0
}
case "${trace}" in *"curve=original|order=021"*) true ;; *) false ;; esac || {
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
echo "ok 1 - native gamma para0 green swap generic"
exit 0
