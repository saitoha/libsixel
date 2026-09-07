#!/bin/sh
# Verify transparent policy preserves alpha zero even when -B is present.

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

transparent_output=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --transparent-policy=transparent -B '#ffffff' \
    -L builtin! -d fs:scan=raster -o - "${input_image}") || {
    echo "not ok" 1 - "transparent transparent-policy render failed"
    exit 0
}
test "${transparent_output#"${esc}P0;1q"}" != \
    "${transparent_output}" || {
    echo "not ok" 1 - "transparent policy did not emit P2=1"
    exit 0
}

echo "ok" 1 - "transparent policy preserves alpha zero with -B"
exit 0
