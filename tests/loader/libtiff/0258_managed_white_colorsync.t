#!/bin/sh
# Policy: docs/loader/color-management.md
# Verify managed white colorsync with independent values or qualified failures.
set -eux
export SIXEL_LOADER_CMS_COLORSYNC_NATIVE_GAMMA=0
result=0
trace=$(SIXEL_TRACE_TOPIC=loader,loader_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    loader/0258_managed_white_colorsync \
    "${TOP_SRCDIR}/tests/data/color-numeric/tiff-managed-white.tif" 2>&1) || result=$?
test "$result" -ne 77 || {
    echo "1..0 # SKIP libtiff or requested CMS is unavailable"
    set -v
    exit 0
}
echo "1..1"
set -v
test "$result" -eq 0 || {
    echo "$trace" >&2
    echo "not ok 1 - managed white colorsync"
    exit 0
}
case "${trace}" in *"TMVALUE|reference=1|case-begin"*) true ;; *) false ;; esac || {
    echo "$trace" >&2
    echo "not ok 1 - reference control absent"
    exit 0
}
observed=${trace#*"TMVALUE|reference=1|case-begin"}
case "${observed}" in *"LSXCMS2|event=apply|engine=colorsync|"*) true ;; *) false ;; esac || {
    echo "$trace" >&2
    echo "not ok 1 - requested engine did not apply"
    exit 0
}
case "${observed}" in *"|ok=1"*) true ;; *) false ;; esac || {
    echo "$trace" >&2
    echo "not ok 1 - unexpected native apply result"
    exit 0
}
case "${observed}" in *"path=gray-pure-gamma|tag=kTRC|from=para0|to=para3"*) true ;; *) false ;; esac || {
    echo "$trace" >&2
    echo "not ok 1 - unexpected gray preparation"
    exit 0
}
echo "ok 1 - managed white colorsync"
exit 0
