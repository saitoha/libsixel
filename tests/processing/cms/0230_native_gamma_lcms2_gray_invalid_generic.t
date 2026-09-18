#!/bin/sh
# Policy: docs/loader/color-management.md
# native gamma lcms2 gray invalid generic compares original-tag native behavior on this OS.
set -eux
result=0
trace=$(SIXEL_TRACE_TOPIC=loader,loader_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    cms/0195_native_gamma curv 11 generic 3 2 lcms2 2>&1) || result=$?
test "$result" -ne 77 || {
    echo "1..0 # SKIP requested CMS engine is unavailable"
    set -v
    exit 0
}
echo "1..1"
set -v
test "$result" -eq 0 || {
    echo "$trace" >&2
    echo "not ok 1 - native gamma lcms2 gray invalid generic"
    exit 0
}
case "${trace}" in *"LSXCMS2|event=apply|engine=lcms2|"*) true ;; *) false ;; esac || {
    echo "$trace" >&2
    echo "not ok 1 - requested engine did not apply"
    exit 0
}
case "${trace}" in *"curve=original|order=012"*) false ;; *) true ;; esac || {
    echo "$trace" >&2
    echo "not ok 1 - unexpected original-curve branch"
    exit 0
}
actual=${trace#*'NGPHASE|begin
'}
actual=${actual%%'NGPHASE|end'*}
case "${actual}" in *"LSXCMS3|event=gamma-source|"*) false ;; *) true ;; esac || {
    echo "$trace" >&2
    echo "not ok 1 - native TRC XYZ or sample bytes changed"
    exit 0
}
echo "ok 1 - native gamma lcms2 gray invalid generic"
exit 0
