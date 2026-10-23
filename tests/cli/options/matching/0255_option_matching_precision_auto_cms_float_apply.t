#!/bin/sh
# Verify precision-policy=auto keeps float application after CMS.
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
    -L 'builtin:cms_engine=builtin!' --precision-policy=auto \
    -o/dev/null "${input_image}" 2>&1) || {
    echo "not ok 1 - automatic CMS conversion failed"
    printf '# %s\n' "${trace}"
    exit 0
}
case "${trace}" in *'selected=lookup/vptree.float32'*) true ;; *) false ;; esac || {
    echo "not ok 1 - automatic precision narrowed CMS output"
    printf '# %s\n' "${trace}"
    exit 0
}

echo "ok 1 - automatic precision keeps float application after CMS"
exit 0
