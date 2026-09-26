#!/bin/sh
# Policy: docs/loader/color-management/README.md
# Verify original TIFF samples, the requested CMS, and the public frame.
set -eux
export SIXEL_LOADER_CMS_COLORSYNC_NATIVE_GAMMA=0
result=0
trace=$(SIXEL_TRACE_TOPIC=loader,loader_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    loader/libtiff/0142_rgb16_source builtin 8 1 \
    "${TOP_SRCDIR}/tests/data/color-numeric/tiff-rgb16-invalid.tif" 2>&1) || result=$?
test "$result" -ne 77 || {
    echo "1..0 # SKIP requested loader or CMS engine is unavailable"
    set -v
    exit 0
}
echo "1..1"
set -v
test "$result" -eq 0 || {
    echo "$trace" >&2
    echo "not ok 1 - RGB16 invalid icc builtin"
    exit 0
}
case "${trace}" in *"LSXLOAD1|event=ok|loader=libtiff|"*) true ;; *) false ;; esac || {
    echo "$trace" >&2
    echo "not ok 1 - libtiff did not deliver the frame"
    exit 0
}
case "${trace}" in *"LSXCMS2|event=apply|"*) false ;; *) true ;; esac || {
    echo "$trace" >&2
    echo "not ok 1 - unmanaged source unexpectedly passed through CMS"
    exit 0
}
echo "ok 1 - RGB16 invalid icc builtin"
exit 0
