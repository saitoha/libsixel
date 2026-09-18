#!/bin/sh
# Policy: docs/loader/color-management.md
# Verify managed black late builtin with independent values or qualified failures.
set -eux
export SIXEL_LOADER_CMS_COLORSYNC_NATIVE_GAMMA=0
result=0
trace=$(SIXEL_TRACE_TOPIC=loader,loader_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    loader/0264_managed_black_late_builtin \
    "${TOP_SRCDIR}/tests/data/color-numeric/tiff-managed-black.tif" 2>&1) || result=$?
test "$result" -ne 77 || {
    echo "1..0 # SKIP libtiff or requested CMS is unavailable"
    set -v
    exit 0
}
echo "1..1"
set -v
test "$result" -eq 0 || {
    echo "$trace" >&2
    echo "not ok 1 - managed black late builtin"
    exit 0
}
observed=${trace#*"TMFAULT|phase-begin=1"}
observed=${observed%%"TMFAULT|phase-begin=2"*}
case "${observed}" in *"LSXCMS2|event=apply|engine=builtin|"*) true ;; *) false ;; esac || {
    echo "$trace" >&2
    echo "not ok 1 - requested engine did not apply"
    exit 0
}
case "${observed}" in *"|ok=0"*) true ;; *) false ;; esac || {
    echo "$trace" >&2
    echo "not ok 1 - unexpected native apply result"
    exit 0
}
echo "ok 1 - managed black late builtin"
exit 0
