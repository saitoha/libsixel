#!/bin/sh
# Keep ordered unrounded fixed entries and FS selection with center; skip construction.
# Policy: docs/encoder/palette-construction/external-palettes.md
set -eux

echo "1..1"
set -v

trace=$(set +xv; SIXEL_TRACE_TOPIC=lookup_contract,dither_contract,palette_contract \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/map_solver/0028_map_fixed_center \
    "${TOP_SRCDIR}/tests/data/inputs/mapfile/p3-fixed.pal" 2>&1) || {
    echo "not ok 1 - map fixed_center"
    printf '# %s\n' "${trace}"
    exit 0
}

case "${trace}" in *"selected=lookup/5bit.float32"*) true ;; *) false ;; esac || {
    echo "not ok 1 - actual map lookup consumer"
    printf '# %s\n' "${trace}"
    exit 0
}

case "${trace}" in *"LSXBIN1|"*) false ;; *) true ;; esac || {
    echo "not ok 1 - fixed map must bypass construction"
    printf '# %s\n' "${trace}"
    exit 0
}

echo "ok 1 - map fixed_center"
exit 0
