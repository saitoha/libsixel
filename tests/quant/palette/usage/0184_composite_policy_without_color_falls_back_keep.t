#!/bin/sh
# Verify composite policy falls back to P2=1 without a resolved color.
# Policy: docs/loader/alpha-policy.md

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v
set +x

input_image="${TOP_SRCDIR}/tests/data/inputs/formats/libpng-minimal-1x1-rgba.png"
esc="$(printf '\033')"

composite_output=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --alpha-policy=composite \
    -L builtin:osc11_query=0! -d fs:scan=raster -o - \
    "${input_image}") || {
    echo "not ok" 1 - "composite alpha-policy render failed"
    exit 0
}
test "${composite_output#"${esc}P0;1q"}" != \
    "${composite_output}" || {
    echo "not ok" 1 - "composite policy did not use the P2=1 fallback"
    exit 0
}

echo "ok" 1 - "composite policy uses P2=1 without a resolved color"
exit 0
