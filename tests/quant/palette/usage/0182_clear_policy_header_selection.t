#!/bin/sh
# Verify clear policy emits P2=0 when it preserves an alpha-zero pixel.
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

clear_output=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --alpha-policy=clear -B '#ffffff' \
    -L builtin! -d fs:scan=raster -o - "${input_image}") || {
    echo "not ok" 1 - "clear alpha-policy render failed"
    exit 0
}
test "${clear_output#"${esc}P0;0q"}" != "${clear_output}" || {
    echo "not ok" 1 - "clear policy did not emit P2=0"
    exit 0
}

echo "ok" 1 - "clear policy emits P2=0"
exit 0
