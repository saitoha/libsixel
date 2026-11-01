#!/bin/sh
# Preserve native map precision from static to animated input.
# Policy: docs/encoder/palette-construction/external-palettes.md
set -eux

echo "1..1"
set -v

trace=$(set +xv; SIXEL_TRACE_TOPIC=lookup_contract,dither_contract \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/map_precision/0023_map_static_to_animation \
    "${TOP_SRCDIR}/tests/data/color-numeric/display-p3.png" 2>&1) || {
    echo "not ok 1 - map static_to_animation"
    printf '# %s\n' "${trace}"
    exit 0
}

case "${trace}" in
    *"selected=lookup/5bit.float32"*"selected=lookup/5bit.float32"*) true ;;
    *) false ;;
esac || {
    echo "not ok 1 - both frames use native consumers"
    printf '# %s\n' "${trace}"
    exit 0
}

echo "ok 1 - map static_to_animation"
exit 0
