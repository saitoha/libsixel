#!/bin/sh
# Policy: docs/loader/color-management.md
# Verify managed white input infinity with independent values or qualified failures.
set -eux
export SIXEL_LOADER_CMS_COLORSYNC_NATIVE_GAMMA=0
result=0
trace=$(SIXEL_TRACE_TOPIC=loader,loader_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    loader/0272_managed_white_input_infinity \
    "${TOP_SRCDIR}/tests/data/color-numeric/tiff-managed-white.tif" \
    "${TOP_SRCDIR}/tests/data/color-numeric/tiff-managed-white-ninf.tif" 2>&1) || result=$?
test "$result" -ne 77 || {
    echo "1..0 # SKIP libtiff or requested CMS is unavailable"
    set -v
    exit 0
}
echo "1..1"
set -v
test "$result" -eq 0 || {
    echo "$trace" >&2
    echo "not ok 1 - managed white input infinity"
    exit 0
}
observed=${trace#*"TMFAULT|phase-begin=1"}
observed=${observed%%"TMFAULT|phase-begin=2"*}
test "$observed" = "${observed#*"LSXCMS2|event=apply"}" || {
    echo "$trace" >&2
    echo "not ok 1 - nonfinite input reached CMS"
    exit 0
}
echo "ok 1 - managed white input infinity"
exit 0
