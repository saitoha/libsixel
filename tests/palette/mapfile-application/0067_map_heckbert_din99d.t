#!/bin/sh
# Retain a CMS-loaded P3 primary and execute heckbert in din99d.
# Policy: docs/encoder/palette-construction/external-palettes.md
set -eux

echo "1..1"
set -v

trace=$(set +xv; SIXEL_TRACE_TOPIC=lookup_contract,dither_contract,palette_contract \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/map_solver/0005_map_heckbert_din99d \
    "${TOP_SRCDIR}/tests/data/color-numeric/display-p3.png" 2>&1) || {
    echo "not ok 1 - map heckbert_din99d"
    printf '# %s\n' "${trace}"
    exit 0
}

case "${trace}" in *"selected=lookup/5bit.float32"*) true ;; *) false ;; esac || {
    echo "not ok 1 - actual map lookup consumer"
    printf '# %s\n' "${trace}"
    exit 0
}

echo "ok 1 - map heckbert_din99d"
exit 0
