#!/bin/sh
# Export sRGB8 from the native map while preserving working coordinates.
# Policy: docs/encoder/palette-construction/external-palettes.md
set -eux

echo "1..1"
set -v

trace=$(set +xv; SIXEL_TRACE_TOPIC=lookup_contract,dither_contract \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/map_precision/0017_map_cms_export \
    "${TOP_SRCDIR}/tests/data/color-numeric/display-p3.png" 2>&1) || {
    echo "not ok 1 - map cms_export"
    printf '# %s\n' "${trace}"
    exit 0
}

case "${trace}" in *"selected=lookup/5bit.float32"*) true ;; *) false ;; esac || {
    echo "not ok 1 - actual float32 consumer"
    printf '# %s\n' "${trace}"
    exit 0
}

echo "ok 1 - map cms_export"
exit 0
