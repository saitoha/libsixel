#!/bin/sh
# Verify precision-policy=8bit rounds an OKLab-clustered palette to bytes.
# Policy: docs/encoder/precision/README.md

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    echo "1..0 # SKIP img2sixel is disabled in this build"
    exit 0
}

echo "1..1"
set -v

input_image="${TOP_SRCDIR}/tests/data/inputs/snake_16.png"
trace=$(set +xv; SIXEL_TRACE_TOPIC=lookup_contract,dither_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --threads=1 \
    -L 'builtin:cms_engine=none!' --precision-policy=8bit -Xoklab \
    -Qkmeans:seed=1 -p16 -o/dev/null "${input_image}" 2>&1) || {
    echo "not ok 1 - 8bit OKLab clustering conversion failed"
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

echo "ok 1 - 8bit rounds an OKLab-clustered palette to bytes"
exit 0
