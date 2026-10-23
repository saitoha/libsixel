#!/bin/sh
# Verify precision-policy=8bit applies a 16-bit source as bytes.
# Policy: docs/encoder/precision/README.md

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    echo "1..0 # SKIP img2sixel is disabled in this build"
    exit 0
}

echo "1..1"
set -v

input_image="${TOP_SRCDIR}/tests/data/inputs/formats/snake-png-gray16.png"
trace=$(set +xv; SIXEL_TRACE_TOPIC=lookup_contract,dither_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --threads=1 \
    -L 'builtin!' --precision-policy=8bit \
    -v -o/dev/null "${input_image}" 2>&1) || {
    echo "not ok 1 - 8bit high-depth conversion failed"
    printf '# %s\n' "${trace}"
    exit 0
}
# The 16-bit source keeps float storage until palette application.
case "${trace}" in *'source=rgb-f32 work=rgb-f32'*) true ;; *) false ;; esac || {
    echo "not ok 1 - high-depth source did not stay float"
    printf '# %s\n' "${trace}"
    exit 0
}
case "${trace}" in *'selected=lookup/vptree.8bit'*) true ;; *) false ;; esac || {
    echo "not ok 1 - palette application did not use the 8bit lookup"
    printf '# %s\n' "${trace}"
    exit 0
}

echo "ok 1 - 8bit applies a 16-bit source as bytes"
exit 0
