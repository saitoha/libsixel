#!/bin/sh
# Verify clear policy omits an alpha-zero pixel and emits P2=0.
# Policy: docs/loader/alpha-policy.md
# Policy: docs/concepts/pixelformat.md

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
expected="${esc}P0;0q\"1;1;1;1#0;2;9;9;9${esc}\\"

clear_output=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --alpha-policy=clear -B '#ffffff' \
    -L builtin! -d fs:scan=raster -o - "${input_image}") || {
    echo "not ok" 1 - "clear alpha-policy render failed"
    exit 0
}
test "${clear_output}" = "${expected}" || {
    echo "not ok" 1 - "clear policy painted the alpha-zero pixel"
    exit 0
}

echo "ok" 1 - "clear policy omits alpha zero and emits P2=0"
exit 0
