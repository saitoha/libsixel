#!/bin/sh
# Verify the default alpha policy is composite.

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
    echo "not ok" 1 - "default alpha-policy render failed"
    exit 0
}
composite_output=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --alpha-policy=composite -B '#ffffff' \
    -L builtin! -d fs:scan=raster -o - "${input_image}") || {
    echo "not ok" 1 - "composite alpha-policy render failed"
    exit 0
}

test "${default_output}" = "${composite_output}" || {
    echo "not ok" 1 - "default alpha-policy is not composite"
    exit 0
}

echo "ok" 1 - "composite is the default alpha-policy"
exit 0
