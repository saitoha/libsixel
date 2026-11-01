#!/bin/sh
# Keep genuine byte map application with heckbert.
# Policy: docs/encoder/palette-construction/external-palettes.md
set -eux

echo "1..1"
set -v

trace=$(set +xv; SIXEL_TRACE_TOPIC=lookup_contract,dither_contract,palette_contract \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/map_solver/0031_map_byte_heckbert \
    "${TOP_SRCDIR}/tests/data/inputs/mapfile/p3-fixed.pal" 2>&1) || {
    echo "not ok 1 - map byte_heckbert"
    printf '# %s\n' "${trace}"
    exit 0
}

case "${trace}" in *"selected=lookup/5bit.8bit"*) true ;; *) false ;; esac || {
    echo "not ok 1 - actual map lookup consumer"
    printf '# %s\n' "${trace}"
    exit 0
}

echo "ok 1 - map byte_heckbert"
exit 0
