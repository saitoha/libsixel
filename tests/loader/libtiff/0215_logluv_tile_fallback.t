#!/bin/sh
# Policy: docs/loader/color-management.md
# Verify logluv tile fallback.
set -eux
result=0
trace=$(SIXEL_TRACE_TOPIC=loader,loader_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    loader/0215_logluv_tile_fallback \
    "${TOP_SRCDIR}/tests/data/color-numeric/tiff-logluv32-tile.tif" 2>&1) || result=$?
test "$result" -ne 77 || {
    echo "1..0 # SKIP required loader or CMS engine is unavailable"
    set -v
    exit 0
}
echo "1..1"
set -v
test "$result" -eq 0 || {
    echo "$trace" >&2
    echo "not ok 1 - logluv tile fallback"
    exit 0
}
case "${trace}" in *"TLOGLUV|native-begin"*) true ;; *) false ;; esac || {
    echo "$trace" >&2
    echo "not ok 1 - native route marker is absent"
    exit 0
}
control=${trace%%"TLOGLUV|native-begin"*}
case "${control}" in *"LSXCMS2|event=apply|engine=builtin|"*) true ;; *) false ;; esac || {
    echo "$trace" >&2
    echo "not ok 1 - positive CMS trace control did not apply"
    exit 0
}
case "${control}" in *"|ok=1"*) true ;; *) false ;; esac || {
    echo "$trace" >&2
    echo "not ok 1 - positive CMS control was unsuccessful"
    exit 0
}
native=${trace#*"TLOGLUV|native-begin"}
case "${native}" in *"LSXCMS2|event=apply"*) false ;; *) true ;; esac || {
    echo "$trace" >&2
    echo "not ok 1 - native XYZ was reinterpreted by CMS"
    exit 0
}
case "${native}" in *"LSXLOAD1|event=ok|loader=libtiff|"*) true ;; *) false ;; esac || {
    echo "$trace" >&2
    echo "not ok 1 - libtiff did not deliver the frame"
    exit 0
}
echo "ok 1 - logluv tile fallback"
exit 0
