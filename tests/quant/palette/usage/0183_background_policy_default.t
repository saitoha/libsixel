#!/bin/sh
# Verify the default alpha policy is background composition.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v
set +x

input_image="${TOP_SRCDIR}/tests/data/inputs/formats/libpng-minimal-1x1-rgba.png"

default_output=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -B '#ffffff' -L builtin! -d fs:scan=raster \
    -o - "${input_image}") || {
    echo "not ok" 1 - "default transparent-policy render failed"
    exit 0
}
background_output=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --transparent-policy=background -B '#ffffff' \
    -L builtin! -d fs:scan=raster -o - "${input_image}") || {
    echo "not ok" 1 - "background transparent-policy render failed"
    exit 0
}

test "${default_output}" = "${background_output}" || {
    echo "not ok" 1 - "default transparent-policy is not background"
    exit 0
}

echo "ok" 1 - "background is the default transparent-policy"
exit 0
