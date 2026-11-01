#!/bin/sh
# Resolve none/hard/none AUTO image-map construction afresh on one encoder.
# Policy: docs/encoder/palette-construction/external-palettes.md
set -eux

echo "1..1"
set -v

trace=$(set +xv; SIXEL_TRACE_TOPIC=lookup_contract,dither_contract,palette_contract \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/map_solver/0034_map_auto_reuse \
    "${TOP_SRCDIR}/tests/data/color-numeric/display-p3.png" 2>&1) || {
    echo "not ok 1 - map auto_reuse"
    printf '# %s\n' "${trace}"
    exit 0
}

case "${trace}" in *"selected=lookup/5bit.float32"*) true ;; *) false ;; esac || {
    echo "not ok 1 - actual map lookup consumer"
    printf '# %s\n' "${trace}"
    exit 0
}

echo "ok 1 - map auto_reuse"
exit 0
