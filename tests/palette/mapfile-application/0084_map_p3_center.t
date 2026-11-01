#!/bin/sh
# Retain native entries with center and Display P3 wire output.
# Policy: docs/encoder/palette-construction/external-palettes.md
set -eux

echo "1..1"
set -v

trace=$(set +xv; SIXEL_TRACE_TOPIC=lookup_contract,dither_contract,palette_contract \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/map_solver/0022_map_p3_center \
    "${TOP_SRCDIR}/tests/data/color-numeric/display-p3.png" 2>&1) || {
    echo "not ok 1 - map p3_center"
    printf '# %s\n' "${trace}"
    exit 0
}

case "${trace}" in *"selected=lookup/5bit.float32"*) true ;; *) false ;; esac || {
    echo "not ok 1 - actual map lookup consumer"
    printf '# %s\n' "${trace}"
    exit 0
}

echo "ok 1 - map p3_center"
exit 0
