#!/bin/sh
# Policy: docs/loader/color-management/README.md
# Verify original TIFF samples, the requested CMS, and the public frame.
set -eux
export SIXEL_LOADER_CMS_COLORSYNC_NATIVE_GAMMA=0
result=0
trace=$(SIXEL_TRACE_TOPIC=loader,loader_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    loader/libtiff/0142_rgb16_source colorsync 3 0 \
    "${TOP_SRCDIR}/tests/data/color-numeric/tiff-rgb16-p3-o4.tif" 2>&1) || result=$?
test "$result" -ne 77 || {
    echo "1..0 # SKIP requested loader or CMS engine is unavailable"
    set -v
    exit 0
}
echo "1..1"
set -v
test "$result" -eq 0 || {
    echo "$trace" >&2
    echo "not ok 1 - RGB16 orientation 4 colorsync"
    exit 0
}
case "${trace}" in *"LSXLOAD1|event=ok|loader=libtiff|"*) true ;; *) false ;; esac || {
    echo "$trace" >&2
    echo "not ok 1 - libtiff did not deliver the frame"
    exit 0
}
case "${trace}" in *"LSXCMS2|event=apply|engine=colorsync|path=external|src=4|dst=4|ok=1"*) true ;; *) false ;; esac || {
    echo "$trace" >&2
    echo "not ok 1 - requested CMS did not apply"
    exit 0
}
echo "ok 1 - RGB16 orientation 4 colorsync"
exit 0
