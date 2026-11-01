#!/bin/sh
# Verify fixed-map native precision: export gamma.
# Policy: docs/encoder/palette-construction/external-palettes.md
set -eux

echo "1..1"
set -v

trace=$(set +xv; SIXEL_TRACE_TOPIC=lookup_contract,dither_contract \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/map_float/0023_map_export_gamma \
    "${TOP_SRCDIR}/tests/data/inputs/mapfile/p3-fixed.pal" 2>&1) || {
    echo "not ok 1 - map export gamma"
    printf '# %s\n' "${trace}"
    exit 0
}

case "${trace}" in *"selected=lookup/5bit.float32"*) true ;; *) false ;; esac || {
    echo "not ok 1 - actual map lookup consumer"
    printf '# %s\n' "${trace}"
    exit 0
}

echo "ok 1 - map export gamma"
exit 0
