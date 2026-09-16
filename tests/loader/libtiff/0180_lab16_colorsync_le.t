#!/bin/sh
# Policy: docs/loader/color-management.md
# Verify raw signed TIFF Lab16 words through the public float frame.
set -eux
result=0
trace=$(SIXEL_LOADER_CMS_RENDERING_INTENT=relative! SIXEL_TRACE_TOPIC=loader,loader_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/0174_lab16_signed_values 3 "${TOP_SRCDIR}/tests/data/color-numeric/tiff-lab16-le-icc.tif" 2>&1) || result=$?
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
test "${trace#*"LSXCMS2|event=apply|engine=colorsync|path=external|src=8|dst=4|ok=1"}" != "$trace" || {
    echo "$trace" >&2
    echo "not ok 1 - expected CMS apply did not execute"
    exit 0
}
test "${trace#*"LAB16|mode=3|calls=1|misses=0|verified=1"}" != "$trace" || {
    echo "$trace" >&2
    echo "not ok 1 - independent frame values were not checked"
    exit 0
}
test "${trace#*"LSXLOAD1|event=ok|loader=libtiff|"}" != "$trace" || {
    echo "$trace" >&2
    echo "not ok 1 - libtiff did not deliver the expected frame"
    exit 0
}
echo "ok 1 - 0180_lab16_colorsync_le public signed Lab16 values"
exit 0
