#!/bin/sh
# Round a managed map primary to bytes under precision-policy=8bit.
# Policy: docs/encoder/palette-construction/external-palettes.md
set -eux

echo "1..1"
set -v

trace=$(set +xv; SIXEL_TRACE_TOPIC=lookup_contract,dither_contract \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/map_precision/0001_map_cms_gamma \
    "${TOP_SRCDIR}/tests/data/color-numeric/display-p3.png" 2>&1) || {
    echo "not ok 1 - map cms_gamma"
    printf '# %s\n' "${trace}"
    exit 0
}

case "${trace}" in *"selected=lookup/5bit.8bit"*) true ;; *) false ;; esac || {
    echo "not ok 1 - actual 8bit consumer"
    printf '# %s\n' "${trace}"
    exit 0
}

echo "ok 1 - map cms_gamma"
exit 0
