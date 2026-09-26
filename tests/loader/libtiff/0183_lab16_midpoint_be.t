#!/bin/sh
# Policy: docs/loader/color-management/README.md
# Verify raw signed TIFF Lab16 words through the public float frame.
set -eux
result=0
trace=$(SIXEL_LOADER_CMS_RENDERING_INTENT=relative! SIXEL_TRACE_TOPIC=loader,loader_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/0174_lab16_signed_values 4 "${TOP_SRCDIR}/tests/data/color-numeric/tiff-lab16-be-mid.tif" 2>&1) || result=$?
test "$result" -ne 77 || {
    echo "1..0 # SKIP required loader or CMS engine unavailable"
    set -v
    exit 0
}
echo "1..1"
set -v
test "$result" -eq 0 || {
    echo "$trace" >&2
    echo "not ok 1 - independent signed Lab16 frame values"
    exit 0
}
case "${trace}" in *"LSXCMS2|event=apply|"*) false ;; *) true ;; esac || {
    echo "$trace" >&2
    echo "not ok 1 - unexpected CMS apply for untagged TIFF"
    exit 0
}
case "${trace}" in *"LAB16|mode=4|calls=1|misses=0|verified=1"*) true ;; *) false ;; esac || {
    echo "$trace" >&2
    echo "not ok 1 - independent frame values were not checked"
    exit 0
}
case "${trace}" in *"LSXLOAD1|event=ok|loader=libtiff|"*) true ;; *) false ;; esac || {
    echo "$trace" >&2
    echo "not ok 1 - libtiff did not deliver the expected frame"
    exit 0
}
echo "ok 1 - 0183_lab16_midpoint_be public signed Lab16 values"
exit 0
