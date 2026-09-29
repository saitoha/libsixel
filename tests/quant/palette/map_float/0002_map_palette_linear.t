#!/bin/sh
# Verify fixed-map native precision: palette linear.
# Policy: docs/encoder/palette-construction/external-palettes.md
set -eux

echo "1..1"
set -v

trace=$(set +xv; SIXEL_TRACE_TOPIC=lookup_contract,dither_contract \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/map_float/0002_map_palette_linear \
    "${TOP_SRCDIR}/tests/data/inputs/mapfile/p3-fixed.pal" 2>&1) || {
    echo "not ok 1 - map palette linear"
    printf '# %s\n' "${trace}"
    exit 0
}

case "${trace}" in *"selected=lookup/5bit.float32"*) true ;; *) false ;; esac || {
    echo "not ok 1 - actual map lookup consumer"
    printf '# %s\n' "${trace}"
    exit 0
}

echo "ok 1 - map palette linear"
exit 0
