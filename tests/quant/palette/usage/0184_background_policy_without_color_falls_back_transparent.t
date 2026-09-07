#!/bin/sh
# Verify background policy falls back to P2=1 without a resolved color.

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

background_output=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --transparent-policy=background \
    -L builtin:osc11_query=0! -d fs:scan=raster -o - \
    "${input_image}") || {
    echo "not ok" 1 - "background transparent-policy render failed"
    exit 0
}
test "${background_output#"${esc}P0;1q"}" != \
    "${background_output}" || {
    echo "not ok" 1 - "background policy did not use the P2=1 fallback"
    exit 0
}

echo "ok" 1 - "background policy uses P2=1 without a resolved color"
exit 0
