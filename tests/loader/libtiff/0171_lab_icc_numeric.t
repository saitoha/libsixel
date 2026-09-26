#!/bin/sh
# Policy: docs/loader/color-management/README.md
# Compare original TIFF ICC samples with independent numeric expectations.
set -eux
result=0
trace=$(SIXEL_TRACE_TOPIC=loader,loader_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    cms/0231_tiff_icc_numeric 7 \
    "${TOP_SRCDIR}/tests/data/colormgmt/input/custom/lab_high_precision_icc.tiff" 2>&1) || result=$?
test "$result" -ne 77 || {
    echo "1..0 # SKIP required loader or CMS engine is unavailable"
    set -v
    exit 0
}
echo "1..1"
set -v
test "$result" -eq 0 || {
    echo "$trace" >&2
    echo "not ok 1 - TIFF ICC lab icc numeric"
    exit 0
}
case "${trace}" in *"TIFFICC|mode=7|verified"*) true ;; *) false ;; esac || {
    echo "$trace" >&2
    echo "not ok 1 - independent values were not verified"
    exit 0
}
case "${trace}" in *"LSXCMS2|event=apply|engine=builtin|path=extended-rgb|"*) true ;; *) false ;; esac || {
    echo "$trace" >&2
    echo "not ok 1 - builtin float endpoint did not apply"
    exit 0
}
case "${trace}" in *"LSXLOAD1|event=ok|loader=libtiff|"*) true ;; *) false ;; esac || {
    echo "$trace" >&2
    echo "not ok 1 - libtiff did not deliver the numeric frame"
    exit 0
}
echo "ok 1 - TIFF ICC lab icc numeric"
exit 0
