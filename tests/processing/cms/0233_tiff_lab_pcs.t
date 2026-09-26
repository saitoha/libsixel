#!/bin/sh
# Policy: docs/loader/color-management/README.md
# Compare original TIFF ICC samples with independent numeric expectations.
set -eux
result=0
trace=$(SIXEL_TRACE_TOPIC=loader,loader_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    cms/0231_tiff_icc_numeric 2 \
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
    echo "not ok 1 - TIFF ICC lab pcs"
    exit 0
}
case "${trace}" in *"TIFFICC|mode=2|verified"*) true ;; *) false ;; esac || {
    echo "$trace" >&2
    echo "not ok 1 - independent values were not verified"
    exit 0
}
echo "ok 1 - TIFF ICC lab pcs"
exit 0
