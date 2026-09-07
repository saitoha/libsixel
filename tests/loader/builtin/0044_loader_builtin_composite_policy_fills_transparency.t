#!/bin/sh
# Verify composite policy fills a zero-alpha pixel with the -B color.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v
set +x

input_png="${TOP_SRCDIR}/tests/data/inputs/formats/libpng-minimal-1x1-rgba.png"
esc="$(printf '\033')"
expected="${esc}Pq\"1;1;1;1#0;2;100;100;100#0@${esc}\\"

output=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --alpha-policy=composite -B '#ffffff' \
    -L builtin! -d fs:scan=raster -o - "${input_png}") || {
    echo "not ok" 1 - "builtin composite policy render failed"
    exit 0
}

test "${output}" = "${expected}" || {
    echo "not ok" 1 - "composite policy did not fill alpha zero with -B"
    exit 0
}

echo "ok" 1 - "composite policy fills alpha zero with -B"
exit 0
