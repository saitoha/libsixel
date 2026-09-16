#!/bin/sh
# Policy: docs/loader/color-management.md
# Verify managed gray mixed native with independent values or qualified failures.
set -eux
export SIXEL_LOADER_CMS_COLORSYNC_NATIVE_GAMMA=1
result=0
trace=$(SIXEL_TRACE_TOPIC=loader,loader_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    loader/0275_managed_gray_mixed_native \
    "${TOP_SRCDIR}/tests/data/color-numeric/tiff-managed-mixed.tif" 2>&1) || result=$?
test "$result" -ne 77 || {
    echo "1..0 # SKIP libtiff or requested CMS is unavailable"
    set -v
    exit 0
}
echo "1..1"
set -v
test "$result" -eq 0 || {
    echo "$trace" >&2
    echo "not ok 1 - managed gray mixed native"
    exit 0
}
test "${trace#*"TMVALUE|reference=1|case-begin"}" != "$trace" || {
    echo "$trace" >&2
    echo "not ok 1 - reference control absent"
    exit 0
}
observed=${trace#*"TMVALUE|reference=1|case-begin"}
test "${observed#*"LSXCMS2|event=apply|engine=colorsync|"}" != "$observed" || {
    echo "$trace" >&2
    echo "not ok 1 - requested engine did not apply"
    exit 0
}
test "${observed#*"|ok=1"}" != "$observed" || {
    echo "$trace" >&2
    echo "not ok 1 - unexpected native apply result"
    exit 0
}
test "$observed" = "${observed#*"event=source-profile-adapter"}" || {
    echo "$trace" >&2
    echo "not ok 1 - mixed device domain unexpectedly prepared"
    exit 0
}
echo "ok 1 - managed gray mixed native"
exit 0
