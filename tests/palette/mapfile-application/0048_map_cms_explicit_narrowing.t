#!/bin/sh
# Honor the explicit loader post-CMS byte narrowing boundary.
# Policy: docs/encoder/palette-construction/external-palettes.md
set -eux

echo "1..1"
set -v

trace=$(set +xv; SIXEL_TRACE_TOPIC=lookup_contract,dither_contract \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/map_precision/0010_map_cms_explicit_narrowing \
    "${TOP_SRCDIR}/tests/data/color-numeric/display-p3.png" 2>&1) || {
    echo "not ok 1 - map cms_explicit_narrowing"
    printf '# %s\n' "${trace}"
    exit 0
}

case "${trace}" in *"selected=lookup/5bit.8bit"*) true ;; *) false ;; esac || {
    echo "not ok 1 - actual 8bit consumer"
    printf '# %s\n' "${trace}"
    exit 0
}

echo "ok 1 - map cms_explicit_narrowing"
exit 0
