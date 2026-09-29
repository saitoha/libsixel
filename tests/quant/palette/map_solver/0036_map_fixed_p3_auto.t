#!/bin/sh
# Keep fixed entries and nonzero FS feedback with AUTO and Display P3 output.
# Policy: docs/encoder/palette-construction/external-palettes.md
set -eux

echo "1..1"
set -v

trace=$(set +xv; SIXEL_TRACE_TOPIC=lookup_contract,dither_contract,palette_contract \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/map_solver/0036_map_fixed_p3_auto \
    "${TOP_SRCDIR}/tests/data/inputs/mapfile/p3-fixed.pal" 2>&1) || {
    echo "not ok 1 - map fixed_p3_auto"
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

echo "ok 1 - map fixed_p3_auto"
exit 0
