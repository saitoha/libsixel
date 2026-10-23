#!/bin/sh
# Verify precision-policy=8bit applies a CMS-converted float frame as bytes.
# Policy: docs/encoder/precision/README.md

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    echo "1..0 # SKIP img2sixel is disabled in this build"
    exit 0
}

echo "1..1"
set -v

input_image="${TOP_SRCDIR}/tests/data/inputs/formats/map8_embedded_icc.png"
trace=$(set +xv; SIXEL_TRACE_TOPIC=lookup_contract,dither_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --threading-policy=1 \
    -L 'builtin:cms_engine=builtin!' --precision-policy=8bit \
    -v -o/dev/null "${input_image}" 2>&1) || {
    echo "not ok 1 - 8bit CMS conversion failed"
    printf '# %s\n' "${trace}"
    exit 0
}
# CMS keeps its float output; only palette application narrows.
case "${trace}" in *'source=linear-f32 work=rgb-f32'*) true ;; *) false ;; esac || {
    echo "not ok 1 - CMS output did not stay float before application"
    printf '# %s\n' "${trace}"
    exit 0
}
case "${trace}" in *'selected=lookup/vptree.8bit'*) true ;; *) false ;; esac || {
    echo "not ok 1 - palette application did not use the 8bit lookup"
    printf '# %s\n' "${trace}"
    exit 0
}
case "${trace}" in *'|rgb101=0|'*) true ;; *) false ;; esac || {
    echo "not ok 1 - palette application kept the float RGB101 palette"
    printf '# %s\n' "${trace}"
    exit 0
}

echo "ok 1 - 8bit applies a CMS-converted frame as bytes"
exit 0
