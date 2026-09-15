#!/bin/sh
# Policy: docs/loader/color-management.md
# Verify original TIFF samples, the requested CMS, and the public frame.
set -eux
export SIXEL_LOADER_CMS_COLORSYNC_NATIVE_GAMMA=0
result=0
trace=$(SIXEL_TRACE_TOPIC=loader,loader_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    loader/libtiff/0142_rgb16_source lcms2 0 0 \
    "${TOP_SRCDIR}/tests/data/color-numeric/tiff-rgb16-p3.tif" 2>&1) || result=$?
test "$result" -ne 77 || {
    echo "1..0 # SKIP requested loader or CMS engine is unavailable"
    set -v
    exit 0
}
echo "1..1"
set -v
test "$result" -eq 0 || {
    echo "$trace" >&2
    echo "not ok 1 - RGB16 p3 lcms2"
    exit 0
}
test "${trace#*"LSXLOAD1|event=ok|loader=libtiff|"}" != "$trace" || {
    echo "$trace" >&2
    echo "not ok 1 - libtiff did not deliver the frame"
    exit 0
}
test "${trace#*"LSXCMS2|event=apply|engine=lcms2|path=external|src=4|dst=4|ok=1"}" != "$trace" || {
    echo "$trace" >&2
    echo "not ok 1 - requested CMS did not apply"
    exit 0
}
echo "ok 1 - RGB16 p3 lcms2"
exit 0
