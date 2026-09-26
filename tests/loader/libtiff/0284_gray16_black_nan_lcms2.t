#!/bin/sh
# Policy: docs/loader/color-management/README.md
# Verify gray16 black nan lcms2 with independent values or qualified failures.
set -eux
export SIXEL_LOADER_CMS_COLORSYNC_NATIVE_GAMMA=0
result=0
trace=$(SIXEL_TRACE_TOPIC=loader,loader_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    loader/0284_gray16_black_nan_lcms2 \
    "${TOP_SRCDIR}/tests/data/color-numeric/tiff-managed-gray16-black.tif" 2>&1) || result=$?
test "$result" -ne 77 || {
    echo "1..0 # SKIP libtiff or requested CMS is unavailable"
    set -v
    exit 0
}
echo "1..1"
set -v
test "$result" -eq 0 || {
    echo "$trace" >&2
    echo "not ok 1 - gray16 black nan lcms2"
    exit 0
}
observed=${trace#*"TMFAULT|phase-begin=1"}
observed=${observed%%"TMFAULT|phase-begin=2"*}
case "${observed}" in *"LSXCMS2|event=apply|engine=lcms2|"*) true ;; *) false ;; esac || {
    echo "$trace" >&2
    echo "not ok 1 - requested engine did not apply"
    exit 0
}
case "${observed}" in *"|ok=1"*) true ;; *) false ;; esac || {
    echo "$trace" >&2
    echo "not ok 1 - unexpected native apply result"
    exit 0
}
echo "ok 1 - gray16 black nan lcms2"
exit 0
