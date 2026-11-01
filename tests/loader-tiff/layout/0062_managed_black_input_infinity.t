#!/bin/sh
# Policy: docs/loader/color-management/README.md
# Verify managed black input infinity with independent values or qualified failures.
set -eux
export SIXEL_LOADER_CMS_COLORSYNC_NATIVE_GAMMA=0
result=0
trace=$(SIXEL_TRACE_TOPIC=loader,loader_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    loader/0271_managed_black_input_infinity \
    "${TOP_SRCDIR}/tests/data/color-numeric/tiff-managed-black.tif" \
    "${TOP_SRCDIR}/tests/data/color-numeric/tiff-managed-black-pinf.tif" 2>&1) || result=$?
test "$result" -ne 77 || {
    echo "1..0 # SKIP libtiff or requested CMS is unavailable"
    set -v
    exit 0
}
echo "1..1"
set -v
test "$result" -eq 0 || {
    echo "$trace" >&2
    echo "not ok 1 - managed black input infinity"
    exit 0
}
observed=${trace#*"TMFAULT|phase-begin=1"}
observed=${observed%%"TMFAULT|phase-begin=2"*}
case "${observed}" in *"LSXCMS2|event=apply"*) false ;; *) true ;; esac || {
    echo "$trace" >&2
    echo "not ok 1 - nonfinite input reached CMS"
    exit 0
}
echo "ok 1 - managed black input infinity"
exit 0
