#!/bin/sh
# Verify the default auto alpha policy resolves to clear.
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

default_output=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -B '#ffffff' -L builtin! -d fs:scan=raster \
    -o - "${input_image}") || {
    echo "not ok" 1 - "default alpha-policy render failed"
    exit 0
}
auto_output=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --alpha-policy=auto -B '#ffffff' \
    -L builtin! -d fs:scan=raster -o - "${input_image}") || {
    echo "not ok" 1 - "auto alpha-policy render failed"
    exit 0
}
clear_output=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --alpha-policy=clear -B '#ffffff' \
    -L builtin! -d fs:scan=raster -o - "${input_image}") || {
    echo "not ok" 1 - "clear alpha-policy render failed"
    exit 0
}
composite_output=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --alpha-policy=composite -B '#ffffff' \
    -L builtin! -d fs:scan=raster -o - "${input_image}") || {
    echo "not ok" 1 - "composite alpha-policy render failed"
    exit 0
}

test "${default_output}" = "${auto_output}" || {
    echo "not ok" 1 - "default alpha-policy is not auto"
    exit 0
}
test "${default_output}" = "${clear_output}" || {
    echo "not ok" 1 - "auto alpha-policy did not resolve to clear"
    exit 0
}
test "${default_output}" != "${composite_output}" || {
    echo "not ok" 1 - "auto alpha-policy unexpectedly resolved to composite"
    exit 0
}

echo "ok" 1 - "auto is the default alpha-policy and resolves to clear"
exit 0
